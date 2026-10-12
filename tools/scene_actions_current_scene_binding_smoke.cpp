#include <cassert>

#include "scene_actions_current_scene_binding.h"

namespace binding = nevergone::scene_actions_current_scene_binding;
namespace queue = nevergone::game_scene_render_queue;
namespace resource = nevergone::game_scene_type0_resource;

queue::SceneActionPairCommand command() {
    queue::SceneActionPairCommand out;
    out.source_layer_index = 4u;
    out.source_object_index = 7u;
    out.resource = resource::Selection{resource::Kind::kSceneActionPair, "klhuo-1.png"};
    out.record.first_point_x = 300.0f;
    out.record.first_point_y = 200.0f;
    out.record.trailing_i32 = 9;
    return out;
}

int main() {
    queue::Queue empty;
    auto result = binding::resolve(empty);
    assert(result.status == binding::Status::kNoPair);
    assert(!result.plan.has_value());

    queue::Queue one;
    one.scene_action_pairs.push_back(command());
    result = binding::resolve(one);
    assert(result.status == binding::Status::kReady);
    assert(result.plan.has_value());
    assert(result.plan->source_layer_index == 4u);
    assert(result.plan->source_object_index == 7u);
    assert(result.plan->slots[0].local_x == 300.0f);
    assert(result.plan->slots[0].local_y == 335.0f);
    assert(result.plan->slots[1].local_y == 155.0f);
    assert(result.plan->slots[0].child_z_order == 9);

    queue::Queue many = one;
    many.scene_action_pairs.push_back(command());
    result = binding::resolve(many);
    assert(result.status == binding::Status::kAmbiguousPairs);
    assert(!result.plan.has_value());

    queue::Queue malformed;
    auto bad = command();
    bad.resource.resource_name = "not-klhuo.png";
    malformed.scene_action_pairs.push_back(bad);
    result = binding::resolve(malformed);
    assert(result.status == binding::Status::kInvalidPair);
    assert(!result.plan.has_value());
    return 0;
}
