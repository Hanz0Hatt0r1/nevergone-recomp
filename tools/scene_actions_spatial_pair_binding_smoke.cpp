#include <cassert>
#include <vector>

#include "scene_actions_spatial_pair_binding.h"

namespace binding = nevergone::scene_actions_spatial_pair_binding;
namespace queue_ns = nevergone::game_scene_render_queue;
namespace resource = nevergone::game_scene_type0_resource;
namespace spatial = nevergone::scene_actions_spatial_trigger;

queue_ns::Queue make_queue() {
    queue_ns::Queue queue;
    queue_ns::SceneActionPairCommand pair;
    pair.source_layer_index = 2u;
    pair.source_object_index = 5u;
    pair.resource = {resource::Kind::kSceneActionPair, "klhuo-1.png", true};
    pair.record.first_point_x = 100.0f;
    pair.record.first_point_y = 200.0f;
    pair.record.trailing_i32 = 3;
    queue.scene_action_pairs.push_back(pair);
    queue.scene_action_pair_count = 1u;
    return queue;
}

binding::BoundObject source_object() {
    binding::BoundObject object;
    object.ref = {2u, 4u};
    object.view.event_id_108 = 2001;
    object.view.resource_name = "event-source.png";
    object.view.object_position_x = 0.0f;
    object.view.sprite_bounds = spatial::Rect{0.0f, 0.0f, 20.0f, 20.0f};
    return object;
}

binding::BoundObject pair_object(bool intersects) {
    binding::BoundObject object;
    object.ref = {2u, 5u};
    object.view.event_id_108 = 0;
    object.view.resource_name = "klhuo-1.png";
    object.view.object_position_x = 0.0f;
    object.view.sprite_bounds = intersects
            ? spatial::Rect{10.0f, 10.0f, 20.0f, 20.0f}
            : spatial::Rect{100.0f, 100.0f, 20.0f, 20.0f};
    return object;
}

int main() {
    const auto queue = make_queue();

    {
        std::vector<binding::BoundObject> objects{source_object(), pair_object(true)};
        const auto result = binding::evaluate(queue, 2001, objects);
        assert(result.status == binding::Status::kPairTriggered);
        assert(result.pair_ref.has_value());
        assert(result.pair_ref->layer_index == 2u);
        assert(result.pair_ref->object_index == 5u);
        assert(result.spatial.source_index.has_value());
        assert(*result.spatial.source_index == 0u);
        assert(result.spatial.trigger_indexes.size() == 2u);
        assert(result.spatial.trigger_indexes[1] == 1u);
    }

    {
        std::vector<binding::BoundObject> objects{source_object(), pair_object(false)};
        const auto result = binding::evaluate(queue, 2001, objects);
        assert(result.status == binding::Status::kPairNotTriggered);
        assert(result.pair_ref.has_value());
        assert(result.spatial.trigger_indexes.size() == 1u);
        assert(result.spatial.trigger_indexes[0] == 0u);
    }

    {
        std::vector<binding::BoundObject> objects{source_object(), pair_object(true)};
        const auto result = binding::evaluate(queue, 1999, objects);
        assert(result.status == binding::Status::kSpatialRejected);
        assert(result.spatial.status == spatial::Status::kEventOutsideSceneActionRange);
    }

    {
        auto no_pair = queue;
        no_pair.scene_action_pairs.clear();
        no_pair.scene_action_pair_count = 0u;
        std::vector<binding::BoundObject> objects{source_object(), pair_object(true)};
        const auto result = binding::evaluate(no_pair, 2001, objects);
        assert(result.status == binding::Status::kNoPair);
    }

    return 0;
}
