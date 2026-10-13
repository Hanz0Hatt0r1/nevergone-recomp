#include <cassert>

#include "scene_actions_bound_object_builder.h"

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

    return 0;
}
