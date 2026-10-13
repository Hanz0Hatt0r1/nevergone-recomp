#include <cassert>
#include <cmath>

#include "scene_actions_bound_object_builder.h"

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

int main() {
    namespace builder = nevergone::scene_actions_bound_object_builder;
    namespace spatial = nevergone::scene_actions_spatial_trigger;

    nevergone::game_levels_scene_prefix::ObjectRecord record;
    record.string_value = "klhuo-1.png";
    record.conditional_present = true;
    record.conditional_first_u32 = 2004u;

    builder::GeometryInput geometry;
    geometry.object_position_x = -12.5f;
    geometry.sprite_bounds = spatial::Rect{1.0f, 2.0f, 20.0f, 30.0f};

    const auto built = builder::build(3u, 7u, record, geometry);
    assert(built.ref.layer_index == 3u);
    assert(built.ref.object_index == 7u);
    assert(built.view.event_id_108 == 2004);
    assert(built.view.resource_name == "klhuo-1.png");
    assert(built.view.object_position_x == -12.5f);
    assert(built.view.sprite_bounds.has_value());
    assert(built.view.sprite_bounds->x == 1.0f);
    assert(built.view.sprite_bounds->y == 2.0f);
    assert(built.view.sprite_bounds->width == 20.0f);
    assert(built.view.sprite_bounds->height == 30.0f);

    record.conditional_present = false;
    const auto no_event = builder::build(0u, 0u, record, {});
    assert(no_event.view.event_id_108 == 0);
    assert(!no_event.view.sprite_bounds.has_value());
    assert(no_event.view.object_position_x == 0.0f);

    nevergone::game_scene_render_queue::SceneActionPairCommand command;
    command.layer_z_index = 2u;  // native GameSceneClassLayer ID 3
    command.source_layer_index = 4u;
    command.source_object_index = 9u;
    command.resource = {
        nevergone::game_scene_type0_resource::Kind::kSceneActionPair,
        "klhuo-1.png",
        true,
    };
    command.layer_camera_factor = 0.25f;
    command.record.string_value = "klhuo-1.png";
    command.record.first_point_x = 100.0f;
    command.record.first_point_y = 200.0f;
    command.record.middle_float = 0.0f;
    command.record.second_point_x = 1.0f;
    command.record.second_point_y = 1.0f;
    command.record.conditional_present = true;
    command.record.conditional_first_u32 = 2001u;

    const auto klhuo = builder::build_klhuo(command, 80.0f);
    assert(klhuo.has_value());
    assert(klhuo->ref.layer_index == 4u);
    assert(klhuo->ref.object_index == 9u);
    assert(klhuo->view.event_id_108 == 2001);
    assert(near(klhuo->view.object_position_x, 60.0f));
    assert(klhuo->view.sprite_bounds.has_value());
    assert(near(klhuo->view.sprite_bounds->x, 90.0f));
    assert(near(klhuo->view.sprite_bounds->y, 190.0f));
    assert(near(klhuo->view.sprite_bounds->width, 20.0f));
    assert(near(klhuo->view.sprite_bounds->height, 20.0f));

    command.layer_z_index = 7u;  // native mode 8 forces parent X = 0.
    const auto mode8 = builder::build_klhuo(command, 999.0f);
    assert(mode8.has_value());
    assert(near(mode8->view.object_position_x, 0.0f));

    command.record.string_value = "gkshu01.png";
    assert(!builder::build_klhuo(command, 0.0f).has_value());

    return 0;
}
