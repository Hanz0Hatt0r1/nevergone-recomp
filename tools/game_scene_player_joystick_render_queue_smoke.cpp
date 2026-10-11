#include <cassert>
#include <cmath>
#include <string>

#include "game_scene_direct_asset_requests.h"
#include "game_scene_direct_geometry.h"
#include "game_scene_player_joystick_render_queue.h"

namespace joystick = nevergone::game_scene_player_joystick_render_queue;

namespace {

bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

}  // namespace

int main() {
    nevergone::game_scene_render_queue::Queue base;
    base.source_scene_index = 4u;
    base.guid = "synthetic-scene";

    const auto queue = joystick::with_dock(base);
    assert(queue.sprites.size() == 1u);
    assert(queue.direct_file_count == 1u);
    assert(queue.sprite_frame_lookup_count == 0u);

    const auto& dock = queue.sprites.back();
    assert(dock.resource.kind == nevergone::game_scene_type0_resource::Kind::kDirectFile);
    assert(dock.resource.resource_name == "Dock.png");
    assert(dock.direct_asset_relative_path.has_value());
    assert(*dock.direct_asset_relative_path == "controls_ui/Dock.png");
    assert(near(dock.transform.position_x,
                nevergone::game_scene_player_joystick_input::kCenterX));
    assert(near(dock.transform.position_y,
                nevergone::game_scene_player_joystick_input::kCenterY));
    assert(near(dock.transform.rotation, 0.0f));
    assert(near(dock.transform.scale_x, 1.0f));
    assert(near(dock.transform.scale_y, 1.0f));

    const auto requests = nevergone::game_scene_direct_asset_requests::build(queue);
    assert(requests.requests.size() == 1u);
    assert(requests.requests[0].kind ==
           nevergone::game_scene_direct_asset_requests::Kind::kDirectFile);
    assert(requests.requests[0].sprite_command_index == 0u);
    assert(requests.requests[0].relative_path == "controls_ui/Dock.png");
    assert(!requests.requests[0].player_body);

    // A 100x80 imported Dock.png is centered on the recovered joystick node.
    nevergone::game_scene_direct_geometry::Quad quad;
    assert(nevergone::game_scene_direct_geometry::build_quad(
            dock.transform, 100, 80, 1136, 640, &quad));
    const float left = 2.0f * (204.0f - 50.0f) / 1136.0f - 1.0f;
    const float right = 2.0f * (204.0f + 50.0f) / 1136.0f - 1.0f;
    const float top = 2.0f * (173.0f + 40.0f) / 640.0f - 1.0f;
    const float bottom = 2.0f * (173.0f - 40.0f) / 640.0f - 1.0f;
    assert(near(quad.positions[0], left));
    assert(near(quad.positions[1], top));
    assert(near(quad.positions[2], left));
    assert(near(quad.positions[3], bottom));
    assert(near(quad.positions[4], right));
    assert(near(quad.positions[5], top));
    assert(near(quad.positions[6], right));
    assert(near(quad.positions[7], bottom));
    return 0;
}
