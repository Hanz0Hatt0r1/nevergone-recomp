#pragma once

#include <optional>

#include "game_scene_render_queue.h"
#include "scene_action_trigger_plan.h"

namespace nevergone::scene_actions_current_scene_binding {

enum class Status {
    kReady = 0,
    kNoPair,
    kAmbiguousPairs,
    kInvalidPair,
};

struct Result {
    Status status = Status::kNoPair;
    std::optional<scene_action_trigger_plan::Plan> plan;
};

// Resolve the project-owned current-scene render queue to the one retained
// SceneActions pair that can receive the recovered trigger contract. The first
// playable capture currently proves one klhuo pair; multiple retained pairs are
// deliberately not ordered/auto-triggered until native trigger selection for
// that case is recovered.
inline Result resolve(const game_scene_render_queue::Queue& queue) {
    if (queue.scene_action_pairs.empty()) return {Status::kNoPair, std::nullopt};
    if (queue.scene_action_pairs.size() != 1u) {
        return {Status::kAmbiguousPairs, std::nullopt};
    }

    const auto plan = scene_action_trigger_plan::build(queue.scene_action_pairs.front());
    if (!plan.has_value()) return {Status::kInvalidPair, std::nullopt};
    return {Status::kReady, plan};
}

}  // namespace nevergone::scene_actions_current_scene_binding
