#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "trap_actions_document.h"

namespace nevergone::scene_actions_initial_frame {

struct Plan {
    std::size_t primary_record_index = 0;
    std::string atlas_plist;
    std::string frame_name;
    std::uint8_t mode_24e = 0;
};

// SceneActionsSystem::initWithFile() uses the first TrapActionsData primary
// ActionFrameData entry, loads its +0x60 string as a sprite-frame plist and
// resolves its +0x68 string as the initial sprite frame. The klhuo construction
// branch separately proves +0x24e = 0 for born and = 1 for idle.
//
// This plan intentionally stops before transform/timing/playback semantics that
// have not yet been recovered for SceneActionsSystem.
inline std::optional<Plan> build(
        const trap_actions_document::Document& document,
        std::uint8_t mode_24e) {
    if (mode_24e > 1u || document.core.primary.records.empty()) return std::nullopt;
    const auto& first = document.core.primary.records.front();
    if (first.first_string.empty() || first.third_string.empty()) return std::nullopt;

    Plan plan;
    plan.primary_record_index = 0u;
    plan.atlas_plist = first.first_string;
    plan.frame_name = first.third_string;
    plan.mode_24e = mode_24e;
    return plan;
}

}  // namespace nevergone::scene_actions_initial_frame
