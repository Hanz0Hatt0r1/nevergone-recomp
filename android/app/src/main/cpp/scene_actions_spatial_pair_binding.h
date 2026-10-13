#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "game_scene_render_queue.h"
#include "scene_actions_current_scene_binding.h"
#include "scene_actions_spatial_trigger.h"

namespace nevergone::scene_actions_spatial_pair_binding {

struct ObjectRef {
    std::size_t layer_index = 0;
    std::size_t object_index = 0;
};

struct BoundObject {
    ObjectRef ref{};
    scene_actions_spatial_trigger::ObjectView view{};
};

enum class Status {
    kPairTriggered = 0,
    kPairNotTriggered,
    kSpatialRejected,
    kNoPair,
    kAmbiguousPairs,
    kInvalidPair,
    kObjectMapMismatch,
};

struct Result {
    Status status = Status::kSpatialRejected;
    scene_actions_spatial_trigger::Result spatial{};
    std::optional<ObjectRef> pair_ref;
};

inline Status map_binding_status(scene_actions_current_scene_binding::Status status) {
    switch (status) {
        case scene_actions_current_scene_binding::Status::kReady:
            break;
        case scene_actions_current_scene_binding::Status::kNoPair:
            return Status::kNoPair;
        case scene_actions_current_scene_binding::Status::kAmbiguousPairs:
            return Status::kAmbiguousPairs;
        case scene_actions_current_scene_binding::Status::kInvalidPair:
            return Status::kInvalidPair;
    }
    return Status::kInvalidPair;
}

// Compose the recovered showEventObjTip()/isContainsRect() scan with the
// already-retained current-scene klhuo pair identity. `objects` must preserve
// the same layer/object traversal order used to build the ObjectView array.
// This helper intentionally does not derive event IDs or sprite bounds: those
// remain explicit evidence boundaries from scene-object state into ObjectView.
inline Result evaluate(
        const game_scene_render_queue::Queue& queue,
        std::int32_t event_id,
        const std::vector<BoundObject>& objects) {
    Result out;

    const auto binding = scene_actions_current_scene_binding::resolve(queue);
    if (binding.status != scene_actions_current_scene_binding::Status::kReady ||
            !binding.plan.has_value()) {
        out.status = map_binding_status(binding.status);
        return out;
    }

    std::vector<scene_actions_spatial_trigger::ObjectView> views;
    views.reserve(objects.size());
    for (const auto& object : objects) views.push_back(object.view);

    out.spatial = scene_actions_spatial_trigger::dispatch(event_id, views);
    if (out.spatial.status != scene_actions_spatial_trigger::Status::kReady) {
        out.status = Status::kSpatialRejected;
        return out;
    }

    const ObjectRef pair{
        binding.plan->source_layer_index,
        binding.plan->source_object_index,
    };
    out.pair_ref = pair;

    for (const std::size_t trigger_index : out.spatial.trigger_indexes) {
        if (trigger_index >= objects.size()) {
            out.status = Status::kObjectMapMismatch;
            return out;
        }
        const ObjectRef& candidate = objects[trigger_index].ref;
        if (candidate.layer_index == pair.layer_index &&
                candidate.object_index == pair.object_index) {
            out.status = Status::kPairTriggered;
            return out;
        }
    }

    out.status = Status::kPairNotTriggered;
    return out;
}

}  // namespace nevergone::scene_actions_spatial_pair_binding
