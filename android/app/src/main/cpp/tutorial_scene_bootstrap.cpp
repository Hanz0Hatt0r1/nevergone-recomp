#include "tutorial_scene_bootstrap.h"

#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>

#include "filesystem_bindings.h"
#include "game_clock.h"
#include "game_levels_enter_transition.h"
#include "game_levels_runtime_state.h"
#include "offline_startup_flow.h"
#include "tutorial_session_state.h"

namespace nevergone::tutorial_scene_bootstrap {
namespace {

constexpr const char* kTutorialSceneFile = "cp_lesson.glData";

std::mutex g_mutex;
Snapshot g_state;

std::string relative_from_assets(
        const std::string& files_dir,
        const std::string& resolved_path) {
    if (files_dir.empty() || resolved_path.empty()) return {};
    const std::filesystem::path root =
        (std::filesystem::path(files_dir) / "assets").lexically_normal();
    const std::filesystem::path value =
        std::filesystem::path(resolved_path).lexically_normal();
    const std::filesystem::path relative = value.lexically_relative(root);
    if (relative.empty()) return value.filename().generic_string();
    for (const auto& part : relative) {
        if (part == "..") return value.filename().generic_string();
    }
    return relative.generic_string();
}

bool runtime_matches(
        std::uint64_t expected_load_attempt,
        const std::string& expected_guid) {
    const auto runtime = game_levels_runtime_state::snapshot();
    return runtime.status == game_levels_runtime_state::LoadStatus::kReady &&
           runtime.current_scene_render_queue_ready &&
           runtime.load_attempt_count == expected_load_attempt &&
           runtime.current_scene_guid == expected_guid;
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
}

bool prepare(const std::string& files_dir) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state = Snapshot{};
        g_state.stage = Stage::kResolvingScene;
        ++g_state.prepare_attempt_count;
    }

    if (files_dir.empty()) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.stage = Stage::kFilesDirUnconfigured;
        return false;
    }

    // GameLevels::LoadGameLevels(char const*) asks CCFileUtils for the full
    // path of the bare filename. lua_runtime::resolve_resource_path() already
    // preserves the recovered AppDelegate::AddAllSearchPath ordering, so reuse
    // that exact clean-room contract rather than guessing a directory or doing
    // a recursive basename scan.
    const std::string resolved = lua_runtime::resolve_resource_path(kTutorialSceneFile);
    std::error_code error;
    if (resolved.empty() ||
            !std::filesystem::is_regular_file(std::filesystem::path(resolved), error) ||
            error) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.stage = Stage::kSceneMissing;
        g_state.resolution = Resolution::kOriginalSearchPaths;
        g_state.scene_relative_path = relative_from_assets(files_dir, resolved);
        return false;
    }

    game_levels_enter_transition::on_local_enter_game_file(resolved);
    const auto enter = game_levels_enter_transition::snapshot();
    const auto runtime = game_levels_runtime_state::snapshot();

    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.resolution = Resolution::kOriginalSearchPaths;
    g_state.scene_relative_path = relative_from_assets(files_dir, resolved);
    g_state.scene_file_size = enter.file_size;
    g_state.verified_bytes = enter.verified_bytes;
    g_state.runtime_load_attempt_count = runtime.load_attempt_count;
    g_state.scene_guid = runtime.current_scene_guid;
    g_state.render_sprite_count = runtime.current_render_sprite_count;

    if (enter.boundary !=
            game_levels_enter_transition::Boundary::kRuntimeRenderQueueReady) {
        if (enter.boundary <=
                game_levels_enter_transition::Boundary::kPortNodeSectionVerified) {
            g_state.stage = Stage::kSceneRejected;
        } else if (runtime.status != game_levels_runtime_state::LoadStatus::kReady) {
            g_state.stage = Stage::kRuntimeLoadFailed;
        } else {
            g_state.stage = Stage::kRuntimeQueueUnready;
        }
        return false;
    }
    if (runtime.status != game_levels_runtime_state::LoadStatus::kReady) {
        g_state.stage = Stage::kRuntimeLoadFailed;
        return false;
    }
    if (!runtime.current_scene_render_queue_ready) {
        g_state.stage = Stage::kRuntimeQueueUnready;
        return false;
    }

    g_state.stage = Stage::kPrepared;
    return true;
}

bool begin_loading() {
    const auto startup = offline_startup_flow::snapshot();
    if (startup.route != offline_startup_flow::Route::kTutorialLoading) return false;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.stage != Stage::kPrepared) return false;
        g_state.fresh_role_generation = startup.fresh_role_local_enter_count;
        ++g_state.loading_start_count;
        g_state.loading_start_game_tick = game_clock::tick_count();
        g_state.current_game_tick = g_state.loading_start_game_tick;
        g_state.stage = Stage::kWaitingForOriginalSwitchDelay;
    }

    tutorial_session_state::begin_loading();
    return true;
}

void tick() {
    const auto startup = offline_startup_flow::snapshot();
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.stage == Stage::kComplete) return;
        if (g_state.stage != Stage::kWaitingForOriginalSwitchDelay) return;
        generation = g_state.fresh_role_generation;
    }

    if (startup.route != offline_startup_flow::Route::kTutorialLoading ||
            startup.fresh_role_local_enter_count != generation) {
        publish_route_changed(generation);
        return;
    }

    const std::uint64_t now = game_clock::tick_count();
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.fresh_role_generation != generation ||
            g_state.stage != Stage::kWaitingForOriginalSwitchDelay) {
        return;
    }
    g_state.current_game_tick = now;
    const std::uint64_t elapsed =
        now >= g_state.loading_start_game_tick
            ? now - g_state.loading_start_game_tick
            : 0u;
    if (elapsed < kOriginalSwitchDelayTicks ||
            g_state.loading_presentation_count == 0u) {
        return;
    }

    ++g_state.switch_callback_due_count;
    g_state.stage = Stage::kWaitingForSceneFrame;
}

void on_loading_frame_presented() {
    const auto startup = offline_startup_flow::snapshot();
    if (startup.route != offline_startup_flow::Route::kTutorialLoading) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stage != Stage::kWaitingForOriginalSwitchDelay &&
            g_state.stage != Stage::kWaitingForSceneFrame) {
        return;
    }
    if (g_state.fresh_role_generation != startup.fresh_role_local_enter_count) return;
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

    if (!runtime_matches(expected_runtime_attempt, expected_guid)) {
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

    // Original switchNextScene initializes the scene, then GameScene::tick()
    // mode 0x0b calls GameSceneUI::showLesson() and immediately asks the UI to
    // hide LoadingLayer with the recovered 0.2 second fade. LessonOverlay is
    // therefore the correct retained tutorial phase at this handoff.
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
        g_state.current_game_tick = game_clock::tick_count();
    }
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* stage_name(Stage stage) {
    switch (stage) {
        case Stage::kIdle: return "idle";
        case Stage::kResolvingScene: return "resolving-scene";
        case Stage::kFilesDirUnconfigured: return "files-dir-unconfigured";
        case Stage::kSceneMissing: return "scene-missing";
        case Stage::kSceneRejected: return "scene-rejected";
        case Stage::kRuntimeLoadFailed: return "runtime-load-failed";
        case Stage::kRuntimeQueueUnready: return "runtime-queue-unready";
        case Stage::kPrepared: return "prepared";
        case Stage::kWaitingForOriginalSwitchDelay: return "waiting-original-switch-delay";
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
        case Resolution::kOriginalSearchPaths: return "original-search-paths";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Tutorial scene bootstrap\n";
    out << "state: " << stage_name(state.stage) << "\n";
    out << "fresh-role generation: " << state.fresh_role_generation << "\n";
    out << "prepare attempts: " << state.prepare_attempt_count << "\n";
    out << "loading starts: " << state.loading_start_count << "\n";
    out << "loading presentations: " << state.loading_presentation_count << "\n";
    out << "original switch delay seconds: " << kOriginalSwitchDelaySeconds << "\n";
    out << "original switch delay ticks: " << kOriginalSwitchDelayTicks << "\n";
    out << "original hide fade seconds: " << kOriginalHideFadeSeconds << "\n";
    if (state.loading_start_count != 0u) {
        out << "loading start game tick: " << state.loading_start_game_tick << "\n";
        out << "current game tick: " << state.current_game_tick << "\n";
        const std::uint64_t elapsed =
            state.current_game_tick >= state.loading_start_game_tick
                ? state.current_game_tick - state.loading_start_game_tick
                : 0u;
        out << "loading elapsed ticks: " << elapsed << "\n";
    }
    out << "switch callbacks due: " << state.switch_callback_due_count << "\n";
    if (state.resolution != Resolution::kNone) {
        out << "scene resolver: " << resolution_name(state.resolution) << "\n";
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
    if (state.stage == Stage::kPrepared ||
            state.stage == Stage::kWaitingForOriginalSwitchDelay ||
            state.stage == Stage::kWaitingForSceneFrame ||
            state.stage == Stage::kComplete) {
        out << "render sprites: " << state.render_sprite_count << "\n";
    }
    out << "scene draw observations: " << state.scene_draw_observation_count << "\n";
    out << "completions: " << state.completion_count << "\n";
    return out.str();
}

}  // namespace nevergone::tutorial_scene_bootstrap
