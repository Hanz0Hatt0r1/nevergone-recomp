#include <cassert>
#include <cmath>
#include <limits>

#include "scene_action_trigger_plan.h"

namespace queue = nevergone::game_scene_render_queue;
namespace resource = nevergone::game_scene_type0_resource;
namespace trigger = nevergone::scene_action_trigger_plan;

int main() {
    queue::SceneActionPairCommand command;
    command.source_layer_index = 7u;
    command.source_object_index = 3u;
    command.resource = resource::Selection{resource::Kind::kSceneActionPair, "klhuo-1.png"};
    command.record.first_point_x = 320.0f;
    command.record.first_point_y = 180.0f;
    command.record.trailing_i32 = -4;

    const auto plan = trigger::build(command);
    assert(plan.has_value());
    assert(plan->source_layer_index == 7u);
    assert(plan->source_object_index == 3u);
    assert(plan->slots[0].slot_index == 0u);
    assert(plan->slots[0].mode_24e == 0u);
    assert(plan->slots[0].local_x == 320.0f);
    assert(plan->slots[0].local_y == 315.0f);
    assert(plan->slots[0].child_z_order == -4);
    assert(plan->slots[1].slot_index == 1u);
    assert(plan->slots[1].mode_24e == 1u);
    assert(plan->slots[1].local_x == 320.0f);
    assert(plan->slots[1].local_y == 135.0f);
    assert(plan->slots[1].child_z_order == -4);

    auto wrong_kind = command;
    wrong_kind.resource.kind = resource::Kind::kSpriteFrameByName;
    assert(!trigger::build(wrong_kind).has_value());

    auto wrong_name = command;
    wrong_name.resource.resource_name = "other.png";
    assert(!trigger::build(wrong_name).has_value());

    auto bad_x = command;
    bad_x.record.first_point_x = std::numeric_limits<float>::quiet_NaN();
    assert(!trigger::build(bad_x).has_value());

    auto bad_y = command;
    bad_y.record.first_point_y = std::numeric_limits<float>::infinity();
    assert(!trigger::build(bad_y).has_value());

    return 0;
}
