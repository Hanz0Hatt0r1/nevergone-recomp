#include "game_levels_enter_transition.h"

#include <mutex>
#include <sstream>

#include "game_levels_runtime_state.h"

namespace nevergone::game_levels_enter_transition {
namespace {
std::mutex g_mutex;
Snapshot g_state;

Boundary classify(const game_levels_asset_probe::Snapshot& probe) {
    if (!probe.configured) return Boundary::kFilesDirUnconfigured;
    if (!probe.present) return Boundary::kAssetMissing;
    if (!probe.regular_file || !probe.within_size_limit || !probe.loaded) return Boundary::kAssetRejected;
    if (!probe.scene_section_readable) return Boundary::kVerifiedPrefixIncomplete;
    if (!probe.actions_section_readable) return Boundary::kSceneSectionVerified;
    if (!probe.global_section_readable) return Boundary::kActionsSectionVerified;
    if (!probe.port_node_section_readable) return Boundary::kGlobalSectionVerified;
    return Boundary::kPortNodeSectionVerified;
}
}  // namespace

void reset() {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state = Snapshot{};
    }
    game_levels_runtime_state::reset();
}

void on_enter_game(const std::string& files_dir) {
    const auto probe = game_levels_asset_probe::probe_pvp_scene(files_dir);
    on_enter_game_with_probe(probe);

    if (!probe.port_node_section_readable) {
        game_levels_runtime_state::reset();
        return;
    }

    if (!game_levels_runtime_state::load_pvp_scene(files_dir)) return;
    const auto runtime = game_levels_runtime_state::snapshot();
    std::lock_guard<std::mutex> lock(g_mutex);
    if (runtime.current_scene_render_queue_ready) {
        g_state.boundary = Boundary::kRuntimeRenderQueueReady;
    } else if (runtime.current_scene_construction_plan_ready) {
        g_state.boundary = Boundary::kRuntimeSceneConstructionPlanReady;
    } else if (runtime.current_scene_instance_ready) {
        g_state.boundary = Boundary::kRuntimeSceneInstanceReady;
    } else {
        g_state.boundary = Boundary::kRuntimeModelReady;
    }
    if (runtime.model_end_offset != 0u) g_state.verified_bytes = runtime.model_end_offset;
}

void on_enter_game_with_probe(const game_levels_asset_probe::Snapshot& probe) {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.enter_callback_count;
    ++g_state.probe_attempt_count;
    g_state.boundary = classify(probe);
    g_state.file_size = probe.file_size;
    g_state.reader_size = probe.reader_size;
    if (probe.port_node_section_readable) {
        g_state.verified_bytes = probe.port_node_section_bytes_consumed;
    } else if (probe.global_section_readable) {
        g_state.verified_bytes = probe.global_section_bytes_consumed;
    } else if (probe.actions_section_readable) {
        g_state.verified_bytes = probe.actions_section_bytes_consumed;
    } else if (probe.scene_section_readable) {
        g_state.verified_bytes = probe.scene_section_bytes_consumed;
    } else {
        g_state.verified_bytes = 0;
    }
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* boundary_name(Boundary boundary) {
    switch (boundary) {
        case Boundary::kIdle: return "idle";
        case Boundary::kFilesDirUnconfigured: return "files-dir-unconfigured";
        case Boundary::kAssetMissing: return "asset-missing";
        case Boundary::kAssetRejected: return "asset-rejected";
        case Boundary::kVerifiedPrefixIncomplete: return "verified-prefix-incomplete";
        case Boundary::kSceneSectionVerified: return "scene-section-verified";
        case Boundary::kActionsSectionVerified: return "actions-section-verified";
        case Boundary::kGlobalSectionVerified: return "global-section-verified";
        case Boundary::kPortNodeSectionVerified: return "port-node-section-verified";
        case Boundary::kRuntimeModelReady: return "runtime-model-ready";
        case Boundary::kRuntimeSceneInstanceReady: return "runtime-scene-instance-ready";
        case Boundary::kRuntimeSceneConstructionPlanReady: return "runtime-scene-construction-plan-ready";
        case Boundary::kRuntimeRenderQueueReady: return "runtime-render-queue-ready";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameLevels enter boundary\n";
    out << "state: " << boundary_name(state.boundary) << "\n";
    out << "enter callbacks: " << state.enter_callback_count << "\n";
    out << "probe attempts: " << state.probe_attempt_count << "\n";
    if (state.reader_size != 0) out << "scene bytes loaded: " << state.reader_size << "\n";
    if (state.verified_bytes != 0) out << "verified GameLevels stream bytes: " << state.verified_bytes << "\n";
    const auto runtime = game_levels_runtime_state::snapshot();
    if (runtime.status != game_levels_runtime_state::LoadStatus::kIdle) {
        out << game_levels_runtime_state::status_report();
    }
    return out.str();
}

}  // namespace nevergone::game_levels_enter_transition
