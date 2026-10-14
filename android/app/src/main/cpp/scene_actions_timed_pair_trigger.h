#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "game_scene_render_queue.h"
#include "scene_actions_current_scene_binding.h"
#include "scene_actions_pair_spatial_hit.h"
#include "scene_actions_spatial_pair_binding.h"
#include "scene_actions_trigger_schedule.h"

namespace nevergone::scene_actions_timed_pair_trigger {

struct State {
    scene_actions_trigger_schedule::State schedule{};
};

enum class Status {
    kNoEvent = 0,
    kPairTriggered,
    kPairNotTriggered,
    kScheduleRejected,
    kNoPair,
    kAmbiguousPairs,
    kInvalidPair,
    kSourceNotFound,
    kPairObjectNotFound,
    kSourceGeometryUnavailable,
    kPairGeometryUnavailable,
    kInvalidGeometry,
};

struct Result {
    Status status = Status::kNoEvent;
    std::optional<std::int32_t> event_id;
    std::optional<std::size_t> event_index;
    std::optional<scene_actions_pair_spatial_hit::Result> spatial;
};

inline Status map_binding_status(scene_actions_current_scene_binding::Status status) {
    switch (status) {
        case scene_actions_current_scene_binding::Status::kReady:
            return Status::kInvalidGeometry;
        case scene_actions_current_scene_binding::Status::kNoPair:
            return Status::kNoPair;
        case scene_actions_current_scene_binding::Status::kAmbiguousPairs:
            return Status::kAmbiguousPairs;
        case scene_actions_current_scene_binding::Status::kInvalidPair:
            return Status::kInvalidPair;
    }
    return Status::kInvalidPair;
}

inline Status map_spatial_status(scene_actions_pair_spatial_hit::Status status) {
    switch (status) {
        case scene_actions_pair_spatial_hit::Status::kPairTriggered:
            return Status::kPairTriggered;
        case scene_actions_pair_spatial_hit::Status::kPairNotTriggered:
            return Status::kPairNotTriggered;
        case scene_actions_pair_spatial_hit::Status::kSourceNotFound:
            return Status::kSourceNotFound;
        case scene_actions_pair_spatial_hit::Status::kPairNotFound:
            return Status::kPairObjectNotFound;
        case scene_actions_pair_spatial_hit::Status::kSourceGeometryUnavailable:
            return Status::kSourceGeometryUnavailable;
        case scene_actions_pair_spatial_hit::Status::kPairGeometryUnavailable:
            return Status::kPairGeometryUnavailable;
        case scene_actions_pair_spatial_hit::Status::kEventOutsideSceneActionRange:
        case scene_actions_pair_spatial_hit::Status::kInvalidGeometry:
            return Status::kInvalidGeometry;
    }
    return Status::kInvalidGeometry;
}

// Reconstruct one TriggerSceneAction(dt) call and decide whether the one
// retained klhuo pair is hit without requiring geometry for unrelated scene
// objects. The schedule is advanced first and keeps its one-shot bit even if
// pair binding or source/pair geometry later fails, matching native dispatch.
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

    const auto scheduled = scene_actions_trigger_schedule::advance(
            dt_seconds, &state->schedule);
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

    const auto binding = scene_actions_current_scene_binding::resolve(queue);
    if (binding.status != scene_actions_current_scene_binding::Status::kReady ||
            !binding.plan.has_value()) {
        out.status = map_binding_status(binding.status);
        return out;
    }

    const scene_actions_spatial_pair_binding::ObjectRef pair_ref{
        binding.plan->source_layer_index,
        binding.plan->source_object_index,
    };
    const auto spatial = scene_actions_pair_spatial_hit::evaluate(
            *scheduled.event_id, pair_ref, objects);
    out.spatial = spatial;
    out.status = map_spatial_status(spatial.status);
    return out;
}

inline void reset(State* state) {
    if (state != nullptr) *state = {};
}

}  // namespace nevergone::scene_actions_timed_pair_trigger
