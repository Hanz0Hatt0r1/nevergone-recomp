#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "trap_actions_document.h"

namespace nevergone::scene_actions_current_frame_resource {

struct Plan {
    std::size_t frame_index = 0;
    std::string atlas_plist;
    std::string frame_name;
    std::uint8_t mode_24e = 0;
};

// Resource-only projection for the current SceneActions primary frame. The
// renderer geometry is composed independently by scene_actions_primary_frame_plan;
// this helper exists so the Android atlas/pixel staging path can resolve later
// animation frames without inventing transform inputs or reparsing .actData.
inline std::optional<Plan> build(
        const trap_actions_document::Document& document,
        std::size_t frame_index,
        std::uint8_t mode_24e) {
    if (mode_24e > 1u || frame_index >= document.core.primary.records.size()) {
        return std::nullopt;
    }
    const auto& frame = document.core.primary.records[frame_index];
    if (frame.first_string.empty() || frame.third_string.empty()) return std::nullopt;

    Plan plan;
    plan.frame_index = frame_index;
    plan.atlas_plist = frame.first_string;
    plan.frame_name = frame.third_string;
    plan.mode_24e = mode_24e;
    return plan;
}

}  // namespace nevergone::scene_actions_current_frame_resource
