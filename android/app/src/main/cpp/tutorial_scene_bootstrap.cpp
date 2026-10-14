#include "tutorial_scene_bootstrap.h"

#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>

#include "game_levels_asset_probe.h"
#include "game_levels_runtime_state.h"
#include "offline_startup_flow.h"
#include "tutorial_session_state.h"

namespace nevergone::tutorial_scene_bootstrap {
namespace {

constexpr const char* kTutorialSceneFile = "cp_lesson.glData";
constexpr std::size_t kMaxSearchDepth = 16u;
constexpr std::size_t kMaxSearchEntries = 8192u;

struct ResolveResult {
    Stage failure = Stage::kIdle;
    Resolution resolution = Resolution::kNone;
    std::filesystem::path absolute_path;
    std::string relative_path;
    std::size_t searched_entry_count = 0;
};

std::mutex g_mutex;
Snapshot g_state;
bool g_loading_route_latched = false;

std::size_t deepest_verified_bytes(const game_levels_asset_probe::Snapshot& probe) {
    if (probe.port_node_section_bytes_consumed != 0u) {
        return probe.port_node_section_bytes_consumed;
    }
    if (probe.global_section_bytes_consumed != 0u) {
        return probe.global_section_bytes_consumed;
    }
    if (probe.actions_section_bytes_consumed != 0u) {
        return probe.actions_section_bytes_consumed;
    }
    if (probe.scene_section_bytes_consumed != 0u) {
        return probe.scene_section_bytes_consumed;
    }
    if (probe.first_scene_layers_bytes_consumed != 0u) {
        return probe.first_scene_layers_bytes_consumed;
    }
    if (probe.first_layer_record_bytes_consumed != 0u) {
        return probe.first_layer_record_bytes_consumed;
    }
    if (probe.first_layer_objects_bytes_consumed != 0u) {
        return probe.first_layer_objects_bytes_consumed;
    }
    return probe.scene_prefix_bytes_consumed;
}

std::string relative_from_root(
        const std::filesystem::path& root,
        const std::filesystem::path& value) {
    const std::filesystem::path relative = value.lexically_relative(root);
    if (!relative.empty()) return relative.generic_string();
    return value.filename().generic_string();
}

ResolveResult resolve_scene(const std::string& files_dir) {
    ResolveResult result;
    if (files_dir.empty()) {
        result.failure = Stage::kFilesDirUnconfigured;
        return result;
    }

    const std::filesystem::path asset_root =
        std::filesystem::path(files_dir) / "assets";
    std::error_code error;
    if (!std::filesystem::is_directory(asset_root, error) || error) {
        result.failure = Stage::kAssetRootUnavailable;
        return result;
    }

    // pvp_scene is known to live under gamescene/gs_list, so try the same
    // conventional GameLevels directory first. The cp_lesson basename itself
    // is the evidence-backed identity; if the imported tree uses another
    // layout, the bounded fallback below resolves that basename instead of
    // claiming this preferred path was observed in the original runtime.
    const std::filesystem::path preferred =
        asset_root / "gamescene" / "gs_list" / kTutorialSceneFile;
    error.clear();
    if (std::filesystem::is_regular_file(preferred, error) && !error) {
        result.resolution = Resolution::kExactPath;
        result.absolute_path = preferred;
        result.relative_path = relative_from_root(asset_root, preferred);
        return result;
    }

    std::filesystem::path match;
    std::filesystem::recursive_directory_iterator it(
        asset_root,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) {
        result.failure = Stage::kAssetRootUnavailable;
        return result;
    }

    for (; it != end;) {
        ++result.searched_entry_count;
        if (result.searched_entry_count > kMaxSearchEntries) {
            result.failure = Stage::kSearchLimitReached;
            return result;
        }

        std::error_code type_error;
        if (it->is_directory(type_error) && !type_error &&
                static_cast<std::size_t>(it.depth()) >= kMaxSearchDepth) {
            it.disable_recursion_pending();
        }

        if (!type_error && it->path().filename() == kTutorialSceneFile) {
            std::error_code regular_error;
            if (it->is_regular_file(regular_error) && !regular_error) {
                const std::filesystem::path candidate = it->path().lexically_normal();
                if (match.empty()) {
                    match = candidate;
                } else if (match != candidate) {
                    result.failure = Stage::kSceneAmbiguous;
                    return result;
                }
            }
        }

        error.clear();
        it.increment(error);
        if (error) {
            result.failure = Stage::kAssetRootUnavailable;
            return result;
        }
    }

    if (match.empty()) {
        result.failure = Stage::kSceneMissing;
        return result;
    }
    result.resolution = Resolution::kRecursiveBasename;
    result.absolute_path = match;
    result.relative_path = relative_from_root(asset_root, match);
    return result;
}

bool still_same_loading_route(std::uint64_t generation) {
    const auto startup = offline_startup_flow::snapshot();
    return startup.route == offline_startup_flow::Route::kTutorialLoading &&
           startup.fresh_role_local_enter_count == generation;
}

void publish_resolution_failure(
        std::uint64_t generation,
        const ResolveResult& resolved) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.fresh_role_generation != generation ||
            g_state.stage != Stage::kResolvingScene) {
        return;
    }
    g_state.stage = resolved.failure;
    g_state.resolution = resolved.resolution;
    g_state.searched_entry_count = resolved.searched_entry_count;
    g_state.scene_relative_path = resolved.relative_path;
}

void publish_route_changed(std::uint64_t generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.fresh_role_generation == generation &&
            g_state.stage != Stage::kComplete) {
        g_state.stage = Stage::kRouteChanged;
    }
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = Snapshot{};
    g_loading_route_latched = false;
}

void tick(const std::string& files_dir) {
    const auto startup = offline_startup_flow::snapshot();
    if (startup.route != offline_startup_flow::Route::kTutorialLoading) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_loading_route_latched = false;
        return;
    }

    bool begin_session = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_loading_route_latched ||
                g_state.fresh_role_generation != startup.fresh_role_local_enter_count) {
            g_state = Snapshot{};
            g_state.stage = Stage::kWaitingForLoadingPresentation;
            g_state.fresh_role_generation = startup.fresh_role_local_enter_count;
            g_loading_route_latched = true;
            begin_session = true;
        }
    }

    if (begin_session) {
        const auto session = tutorial_session_state::snapshot();
        if (session.phase != tutorial_session_state::Phase::kLoading) {
            tutorial_session_state::begin_loading();
        }
    }

    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.stage != Stage::kWaitingForLoadingPresentation ||
                g_state.loading_presentation_count == 0u) {
            return;
        }
        generation = g_state.fresh_role_generation;
        g_state.stage = Stage::kResolvingScene;
        ++g_state.bootstrap_attempt_count;
    }

    const ResolveResult resolved = resolve_scene(files_dir);
    if (resolved.failure != Stage::kIdle) {
        publish_resolution_failure(generation, resolved);
        return;
    }
    if (!still_same_loading_route(generation)) {
        publish_route_changed(generation);
        return;
    }

    const auto probe = game_levels_asset_probe::probe_file(resolved.absolute_path.string());
    const std::size_t verified_bytes = deepest_verified_bytes(probe);
    if (!probe.port_node_section_readable) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.fresh_role_generation == generation &&
                g_state.stage == Stage::kResolvingScene) {
            g_state.stage = Stage::kSceneRejected;
            g_state.resolution = resolved.resolution;
            g_state.searched_entry_count = resolved.searched_entry_count;
            g_state.scene_relative_path = resolved.relative_path;
            g_state.scene_file_size = probe.file_size;
            g_state.verified_bytes = verified_bytes;
        }
        return;
    }
    if (!still_same_loading_route(generation)) {
        publish_route_changed(generation);
        return;
    }

    const bool loaded = game_levels_runtime_state::load_file(
        resolved.absolute_path.string());
    const auto runtime = game_levels_runtime_state::snapshot();
    if (!still_same_loading_route(generation)) {
        publish_route_changed(generation);
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.fresh_role_generation != generation ||
            g_state.stage != Stage::kResolvingScene) {
        return;
    }
    g_state.resolution = resolved.resolution;
    g_state.searched_entry_count = resolved.searched_entry_count;
    g_state.scene_relative_path = resolved.relative_path;
    g_state.scene_file_size = probe.file_size;
    g_state.verified_bytes = verified_bytes;
    g_state.runtime_load_attempt_count = runtime.load_attempt_count;
    g_state.scene_guid = runtime.current_scene_guid;
    g_state.render_sprite_count = runtime.current_render_sprite_count;

    if (!loaded || runtime.status != game_levels_runtime_state::LoadStatus::kReady) {
        g_state.stage = Stage::kRuntimeLoadFailed;
        return;
    }
    if (!runtime.current_scene_render_queue_ready) {
        g_state.stage = Stage::kRuntimeQueueUnready;
        return;
    }
    g_state.stage = Stage::kWaitingForSceneFrame;
}

void on_loading_frame_presented() {
    const auto startup = offline_startup_flow::snapshot();
    if (startup.route != offline_startup_flow::Route::kTutorialLoading) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_loading_route_latched ||
            g_state.fresh_role_generation != startup.fresh_role_local_enter_count ||
            g_state.stage == Stage::kComplete) {
        return;
    }
    ++g_state.loading_presentation_count;
}

void observe_scene_draw(bool drawn) {
    std::uint64_t generation = 0;
    std::uint64_t expected_runtime_attempt = 0;
    std::string expected_guid;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.stage != Stage::kWaitingForSceneFrame) return;
        ++g_state.scene_draw_observation_count;
        if (!drawn) return;
        generation = g_state.fresh_role_generation;
        expected_runtime_attempt = g_state.runtime_load_attempt_count;
        expected_guid = g_state.scene_guid;
    }

    const auto runtime = game_levels_runtime_state::snapshot();
    if (runtime.status != game_levels_runtime_state::LoadStatus::kReady ||
            !runtime.current_scene_render_queue_ready ||
            runtime.load_attempt_count != expected_runtime_attempt ||
            runtime.current_scene_guid != expected_guid) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.fresh_role_generation == generation &&
                g_state.stage == Stage::kWaitingForSceneFrame) {
            g_state.stage = Stage::kRuntimeChanged;
        }
        return;
    }

    const auto startup = offline_startup_flow::snapshot();
    if (startup.route != offline_startup_flow::Route::kTutorialLoading ||
            startup.fresh_role_local_enter_count != generation) {
        publish_route_changed(generation);
        return;
    }

    if (tutorial_session_state::snapshot().phase !=
            tutorial_session_state::Phase::kLoading ||
            !tutorial_session_state::on_loading_complete()) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.fresh_role_generation == generation &&
                g_state.stage == Stage::kWaitingForSceneFrame) {
            g_state.stage = Stage::kSessionTransitionRejected;
        }
        return;
    }

    if (!offline_startup_flow::on_tutorial_loading_complete()) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.fresh_role_generation == generation &&
                g_state.stage == Stage::kWaitingForSceneFrame) {
            g_state.stage = Stage::kRouteTransitionRejected;
        }
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.fresh_role_generation == generation &&
            g_state.stage == Stage::kWaitingForSceneFrame) {
        g_state.stage = Stage::kComplete;
        ++g_state.completion_count;
    }
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* stage_name(Stage stage) {
    switch (stage) {
        case Stage::kIdle: return "idle";
        case Stage::kWaitingForLoadingPresentation: return "waiting-loading-presentation";
        case Stage::kResolvingScene: return "resolving-scene";
        case Stage::kFilesDirUnconfigured: return "files-dir-unconfigured";
        case Stage::kAssetRootUnavailable: return "asset-root-unavailable";
        case Stage::kSceneMissing: return "scene-missing";
        case Stage::kSceneAmbiguous: return "scene-ambiguous";
        case Stage::kSearchLimitReached: return "search-limit-reached";
        case Stage::kSceneRejected: return "scene-rejected";
        case Stage::kRuntimeLoadFailed: return "runtime-load-failed";
        case Stage::kRuntimeQueueUnready: return "runtime-queue-unready";
        case Stage::kWaitingForSceneFrame: return "waiting-scene-frame";
        case Stage::kRuntimeChanged: return "runtime-changed";
        case Stage::kSessionTransitionRejected: return "session-transition-rejected";
        case Stage::kRouteTransitionRejected: return "route-transition-rejected";
        case Stage::kRouteChanged: return "route-changed";
        case Stage::kComplete: return "complete";
    }
    return "unknown";
}

const char* resolution_name(Resolution resolution) {
    switch (resolution) {
        case Resolution::kNone: return "none";
        case Resolution::kExactPath: return "exact-path";
        case Resolution::kRecursiveBasename: return "recursive-basename";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Tutorial scene bootstrap\n";
    out << "state: " << stage_name(state.stage) << "\n";
    out << "fresh-role generation: " << state.fresh_role_generation << "\n";
    out << "bootstrap attempts: " << state.bootstrap_attempt_count << "\n";
    out << "loading presentations: " << state.loading_presentation_count << "\n";
    if (state.resolution != Resolution::kNone) {
        out << "scene resolver: " << resolution_name(state.resolution) << "\n";
    }
    if (state.searched_entry_count != 0u) {
        out << "scene search entries: " << state.searched_entry_count << "\n";
    }
    if (!state.scene_relative_path.empty()) {
        out << "scene file: " << state.scene_relative_path << "\n";
    }
    if (state.scene_file_size != 0u) {
        out << "scene file bytes: " << state.scene_file_size << "\n";
    }
    if (state.verified_bytes != 0u) {
        out << "verified GameLevels bytes: " << state.verified_bytes << "\n";
    }
    if (state.runtime_load_attempt_count != 0u) {
        out << "runtime load attempt: " << state.runtime_load_attempt_count << "\n";
    }
    if (!state.scene_guid.empty()) out << "scene guid: " << state.scene_guid << "\n";
    if (state.stage == Stage::kWaitingForSceneFrame || state.stage == Stage::kComplete) {
        out << "render sprites: " << state.render_sprite_count << "\n";
    }
    out << "scene draw observations: " << state.scene_draw_observation_count << "\n";
    out << "completions: " << state.completion_count << "\n";
    return out.str();
}

}  // namespace nevergone::tutorial_scene_bootstrap
