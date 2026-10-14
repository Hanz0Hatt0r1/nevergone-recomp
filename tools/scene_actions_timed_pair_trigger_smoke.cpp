#include <cassert>

#include "scene_actions_timed_pair_trigger.h"

int main() {
    namespace timed = nevergone::scene_actions_timed_pair_trigger;
    namespace binding = nevergone::scene_actions_spatial_pair_binding;
    namespace spatial = nevergone::scene_actions_spatial_trigger;

    nevergone::game_scene_render_queue::Queue queue;
    nevergone::game_scene_render_queue::SceneActionPairCommand pair_command;
    pair_command.source_layer_index = 2u;
    pair_command.source_object_index = 3u;
    pair_command.resource = nevergone::game_scene_type0_resource::select("klhuo-1.png");
    pair_command.record.string_value = "klhuo-1.png";
    queue.scene_action_pairs.push_back(pair_command);

    binding::BoundObject source;
    source.ref = {2u, 1u};
    source.view.event_id_108 = 2001;
    source.view.resource_name = "source.png";
    source.view.sprite_bounds = spatial::Rect{10.0f, 20.0f, 20.0f, 20.0f};

    binding::BoundObject unresolved;
    unresolved.ref = {2u, 2u};
    unresolved.view.resource_name = "trimmed.png";

    binding::BoundObject pair;
    pair.ref = {2u, 3u};
    pair.view.resource_name = "klhuo-1.png";
    pair.view.sprite_bounds = spatial::Rect{20.0f, 20.0f, 20.0f, 20.0f};

    timed::State state;
    auto result = timed::advance(5.1f, queue, {source, unresolved, pair}, &state);
    assert(result.status == timed::Status::kPairTriggered);
    assert(result.event_id.has_value() && *result.event_id == 2001);
    assert(result.event_index.has_value() && *result.event_index == 0u);
    assert(state.schedule.fired_244_24b[0]);

    // Event 2001 remains consumed even when its spatial decision fails.
    timed::reset(&state);
    source.view.sprite_bounds.reset();
    result = timed::advance(5.1f, queue, {source, pair}, &state);
    assert(result.status == timed::Status::kSourceGeometryUnavailable);
    assert(state.schedule.fired_244_24b[0]);
    result = timed::advance(0.4f, queue, {source, pair}, &state);
    assert(result.event_id.has_value() && *result.event_id == 2002);
    assert(result.status == timed::Status::kSourceNotFound);

    // No retained pair rejects only after the timed event has been consumed.
    timed::reset(&state);
    queue.scene_action_pairs.clear();
    result = timed::advance(5.1f, queue, {source}, &state);
    assert(result.status == timed::Status::kNoPair);
    assert(state.schedule.fired_244_24b[0]);

    return 0;
}
