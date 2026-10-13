#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "game_scene_render_queue.h"
#include "scene_actions_spatial_pair_binding.h"
#include "scene_actions_trigger_schedule.h"

namespace nevergone::scene_actions_timed_spatial_trigger {

struct State {
    scene_actions_trigger_schedule::State schedule{};
};

enum class Status {
    kNoEvent = 0,
    kPairTriggered,
    kPairNotTriggered,
    kScheduleRejected,
    kSpatialRejected,
    kNoPair,
    kAmbiguousPairs,
    kInvalidPair,
    kObjectMapMismatch,
};

struct Result {
    Status status = Status::kNoEvent;
    std::optional<std::int32_t> event_id;
    std::optional<std::size_t> event_index;
    std::optional<scene_actions_spatial_pair_binding::Result> spatial_binding;
};

inline Status map_binding(scene_actions_spatial_pair_binding::Status status) {
    switch (status) {
        case scene_actions_spatial_pair_binding::Status::kPairTriggered:
            return Status::kPairTriggered;
        case scene_actions_spatial_pair_binding::Status::kPairNotTriggered:
            return Status::kPairNotTriggered;
        case scene_actions_spatial_pair_binding::Status::kSpatialRejected:
            return Status::kSpatialRejected;
        case scene_actions_spatial_pair_binding::Status::kNoPair:
            return Status::kNoPair;
        case scene_actions_spatial_pair_binding::Status::kAmbiguousPairs:
            return Status::kAmbiguousPairs;
        case scene_actions_spatial_pair_binding::Status::kInvalidPair:
            return Status::kInvalidPair;
        case scene_actions_spatial_pair_binding::Status::kObjectMapMismatch:
            return Status::kObjectMapMismatch;
    }
    return Status::kSpatialRejected;
}

// Reconstruct one GameScene::TriggerSceneAction(dt) call followed by the
// recovered showEventObjTip(event_id) spatial branch for the current klhuo pair.
// The timed scheduler owns its one-shot bits independently of spatial success:
// once a threshold emits an event, that threshold remains consumed even when
// showEventObjTip finds no matching source or the current pair is not hit.
inline Result advance(
        float dt_seconds,
        const game_scene_render_queue::Queue& queue,
        const std::vector<scene_actions_spatial_pair_binding::BoundObject>& objects,
        State* state) {
    Result out;
    if (state == nullptr) {
        out.status = Status::kScheduleRejected;
        return out;
    }

    const auto scheduled = scene_actions_trigger_schedule::advance(dt_seconds, &state->schedule);
    out.event_id = scheduled.event_id;
    out.event_index = scheduled.event_index;

    if (scheduled.status == scene_actions_trigger_schedule::Status::kRejected) {
        out.status = Status::kScheduleRejected;
        return out;
    }
    if (scheduled.status == scene_actions_trigger_schedule::Status::kNoEvent ||
            !scheduled.event_id.has_value()) {
        out.status = Status::kNoEvent;
        return out;
    }

    const auto bound = scene_actions_spatial_pair_binding::evaluate(
            queue, *scheduled.event_id, objects);
    out.spatial_binding = bound;
    out.status = map_binding(bound.status);
    return out;
}

inline void reset(State* state) {
    if (state != nullptr) *state = {};
}

}  // namespace nevergone::scene_actions_timed_spatial_trigger
