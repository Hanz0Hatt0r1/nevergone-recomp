#include <cassert>
#include <vector>

#include "scene_actions_timed_spatial_trigger.h"

namespace timed = nevergone::scene_actions_timed_spatial_trigger;
namespace binding = nevergone::scene_actions_spatial_pair_binding;
namespace queue_ns = nevergone::game_scene_render_queue;
namespace resource = nevergone::game_scene_type0_resource;
namespace spatial = nevergone::scene_actions_spatial_trigger;

queue_ns::Queue make_queue() {
    queue_ns::Queue queue;
    queue_ns::SceneActionPairCommand pair;
    pair.source_layer_index = 1u;
    pair.source_object_index = 9u;
    pair.resource = {resource::Kind::kSceneActionPair, "klhuo-1.png", true};
    pair.record.first_point_x = 20.0f;
    pair.record.first_point_y = 30.0f;
    pair.record.trailing_i32 = 2;
    queue.scene_action_pairs.push_back(pair);
    queue.scene_action_pair_count = 1u;
    return queue;
}

std::vector<binding::BoundObject> make_objects() {
    binding::BoundObject source;
    source.ref = {1u, 8u};
    source.view.event_id_108 = 2001;
    source.view.resource_name = "source.png";
    source.view.sprite_bounds = spatial::Rect{0.0f, 0.0f, 20.0f, 20.0f};

    binding::BoundObject pair;
    pair.ref = {1u, 9u};
    pair.view.resource_name = "klhuo-1.png";
    pair.view.sprite_bounds = spatial::Rect{10.0f, 10.0f, 20.0f, 20.0f};

    return {source, pair};
}

int main() {
    const auto queue = make_queue();
    const auto objects = make_objects();
    timed::State state;

    // Native threshold comparison is strict; exactly five seconds does not fire.
    auto result = timed::advance(5.0f, queue, objects, &state);
    assert(result.status == timed::Status::kNoEvent);
    assert(!result.event_id.has_value());

    // Crossing the first threshold emits 2001 and the current klhuo pair is hit.
    result = timed::advance(0.01f, queue, objects, &state);
    assert(result.status == timed::Status::kPairTriggered);
    assert(result.event_id.has_value() && *result.event_id == 2001);
    assert(result.event_index.has_value() && *result.event_index == 0u);
    assert(result.spatial_binding.has_value());
    assert(result.spatial_binding->status == binding::Status::kPairTriggered);

    // The first event remains consumed. Advancing past 5.4 emits 2002 once;
    // our synthetic object set has no 2002 source, so the spatial phase rejects
    // while the schedule bit remains fired exactly as the native call chain does.
    result = timed::advance(0.40f, queue, objects, &state);
    assert(result.status == timed::Status::kSpatialRejected);
    assert(result.event_id.has_value() && *result.event_id == 2002);
    assert(state.schedule.fired_244_24b[0]);
    assert(state.schedule.fired_244_24b[1]);

    // Same accumulated time cannot re-emit 2002; the next eligible event is 2003
    // only after its own strict threshold is crossed.
    result = timed::advance(0.0f, queue, objects, &state);
    assert(result.status == timed::Status::kNoEvent);

    timed::reset(&state);
    assert(state.schedule.accumulated_seconds_240 == 0.0f);
    for (bool fired : state.schedule.fired_244_24b) assert(!fired);

    assert(timed::advance(1.0f, queue, objects, nullptr).status ==
            timed::Status::kScheduleRejected);

    return 0;
}
