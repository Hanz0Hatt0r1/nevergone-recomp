#include <cassert>
#include <cmath>

#include "game_scene_direct_spatial_geometry.h"

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

int main() {
    namespace geometry = nevergone::game_scene_direct_spatial_geometry;

    nevergone::game_scene_render_queue::SpriteCommand command;
    command.layer_z_index = 0u;
    command.source_layer_index = 3u;
    command.source_object_index = 7u;
    command.resource = nevergone::game_scene_type0_resource::select("gktianchong.png");
    command.direct_asset_relative_path = "gktianchong.png";
    command.layer_camera_factor = 0.25f;
    command.transform.position_x = 100.0f;
    command.transform.position_y = 200.0f;
    command.transform.scale_x = 1.0f;
    command.transform.scale_y = 1.0f;

    auto entry = geometry::build(command, 0.0f, 20, 40, 40.0f);
    assert(entry.has_value());
    assert(entry->source_layer_index == 3u);
    assert(entry->source_object_index == 7u);
    assert(near(entry->object_position_x, 30.0f));
    assert(near(entry->sprite_bounds.x, 90.0f));
    assert(near(entry->sprite_bounds.y, 180.0f));
    assert(near(entry->sprite_bounds.width, 20.0f));
    assert(near(entry->sprite_bounds.height, 40.0f));

    command.layer_z_index = 7u;
    entry = geometry::build(command, 0.0f, 20, 40, 40.0f);
    assert(entry.has_value());
    assert(near(entry->object_position_x, 0.0f));

    command.layer_z_index = 0u;
    entry = geometry::build(command, 12.0f, 20, 40, 40.0f);
    assert(entry.has_value());
    assert(entry->sprite_bounds.width > 20.0f);

    command.resource = nevergone::game_scene_type0_resource::select("gkshu01.png");
    command.direct_asset_relative_path.reset();
    assert(!geometry::build(command, 0.0f, 20, 40, 40.0f).has_value());

    return 0;
}
