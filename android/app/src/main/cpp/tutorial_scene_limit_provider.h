#pragma once

#include <optional>
#include <sstream>
#include <string>

#include "game_levels_port_node_section.h"
#include "game_levels_runtime_state.h"
#include "tutorial_spawn_inputs.h"

namespace nevergone::tutorial_scene_limit_provider {

struct Resolution {
    tutorial_spawn_inputs::ReviewedSceneLimits scene_limits{};
    bool runtime_ready = false;
    bool port_node_available = false;
    bool port_scene_matches = false;

    bool ready() const {
        return runtime_ready && port_node_available && port_scene_matches &&
            scene_limits.reviewed;
    }
};

// Exact original ARMv7 evidence closes the previously unresolved mapping from
// the three serialized PortNode uint32 values to LessonLayer's scene-limit
// owner fields:
//   third_u32  -> GameScenePortNodeData +0x4ac -> vertical_edge_a
//   fourth_u32 -> GameScenePortNodeData +0x4b0 -> vertical_edge_b
//   fifth_u32  -> GameScenePortNodeData +0x4b4 -> right_edge_x
// The first PortNode string is independently recovered as the scene GUID used
// by GameScene::getGLGameSceneData(), so require it to agree with the retained
// runtime scene before stamping the values as reviewed.
inline Resolution resolve(
        const game_levels_runtime_state::Snapshot& runtime,
        const std::optional<game_levels_port_node_section::PortNodeRecord>& port) {
    Resolution out;
    out.scene_limits.runtime_load_attempt_count = runtime.load_attempt_count;
    out.scene_limits.scene_guid = runtime.current_scene_guid;

    out.runtime_ready =
        runtime.status == game_levels_runtime_state::LoadStatus::kReady &&
        runtime.current_port_node_index.has_value() &&
        !runtime.current_scene_guid.empty();
    if (!out.runtime_ready || !port.has_value()) return out;

    out.port_node_available = true;
    out.port_scene_matches = port->first_string == runtime.current_scene_guid;
    if (!out.port_scene_matches) return out;

    out.scene_limits.vertical_edge_a = port->third_u32;
    out.scene_limits.vertical_edge_b = port->fourth_u32;
    out.scene_limits.right_edge_x = port->fifth_u32;
    out.scene_limits.reviewed = true;
    return out;
}

inline std::string status_report(const Resolution& resolution) {
    std::ostringstream out;
    out << "Tutorial scene-limit provider\n";
    out << "ready: " << (resolution.ready() ? "yes" : "no") << "\n";
    out << "runtime ready: " << (resolution.runtime_ready ? "yes" : "no") << "\n";
    out << "port node available: "
        << (resolution.port_node_available ? "yes" : "no") << "\n";
    out << "port scene matches: "
        << (resolution.port_scene_matches ? "yes" : "no") << "\n";
    out << "runtime load attempt: "
        << resolution.scene_limits.runtime_load_attempt_count << "\n";
    out << "scene guid: " << resolution.scene_limits.scene_guid << "\n";
    out << "reviewed: " << (resolution.scene_limits.reviewed ? "yes" : "no") << "\n";
    if (resolution.scene_limits.reviewed) {
        out << "vertical edge A: " << resolution.scene_limits.vertical_edge_a << "\n";
        out << "vertical edge B: " << resolution.scene_limits.vertical_edge_b << "\n";
        out << "right edge X: " << resolution.scene_limits.right_edge_x << "\n";
    }
    return out.str();
}

}  // namespace nevergone::tutorial_scene_limit_provider
