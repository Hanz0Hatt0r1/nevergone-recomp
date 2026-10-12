#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "game_scene_render_queue.h"

namespace nevergone::scene_action_trigger_plan {

constexpr std::size_t kSlotCount = 2u;
constexpr float kBornYOffset = 135.0f;
constexpr float kIdleYOffset = -45.0f;

struct SlotPlan {
    std::size_t slot_index = 0;
    std::uint8_t mode_24e = 0;
    float local_x = 0.0f;
    float local_y = 0.0f;
    std::int32_t child_z_order = 0;
};

struct Plan {
    std::size_t source_layer_index = 0;
    std::size_t source_object_index = 0;
    std::array<SlotPlan, kSlotCount> slots{};
};

// GameSceneObject::triggerSceneAction() consumes the retained SceneActions
// array in order. For the klhuo pair that order is born (mode 0) then idle
// (mode 1). Both use the serialized object X coordinate; native offsets born Y
// by +135 and idle Y by -45 before attaching each SceneActionsSystem at the
// object's serialized child-Z. This function reconstructs only that placement
// and ordering contract; it does not fire the trigger, select a PRNG variant,
// advance animation time, or assume the systems were attached during init.
inline std::optional<Plan> build(
        const game_scene_render_queue::SceneActionPairCommand& command) {
    if (command.resource.kind != game_scene_type0_resource::Kind::kSceneActionPair ||
        command.resource.resource_name != "klhuo-1.png") {
        return std::nullopt;
    }

    const float x = command.record.first_point_x;
    const float y = command.record.first_point_y;
    if (!std::isfinite(x) || !std::isfinite(y)) return std::nullopt;

    Plan plan;
    plan.source_layer_index = command.source_layer_index;
    plan.source_object_index = command.source_object_index;
    plan.slots[0] = SlotPlan{
        0u,
        0u,
        x,
        y + kBornYOffset,
        command.record.trailing_i32,
    };
    plan.slots[1] = SlotPlan{
        1u,
        1u,
        x,
        y + kIdleYOffset,
        command.record.trailing_i32,
    };
    return plan;
}

}  // namespace nevergone::scene_action_trigger_plan
