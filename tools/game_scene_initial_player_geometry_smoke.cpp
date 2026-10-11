#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include "game_scene_initial_player_geometry.h"

namespace {

bool near(float actual, float expected, float epsilon = 0.0001f) {
    return std::fabs(actual - expected) <= epsilon;
}

nevergone::game_scene_render_queue::SpriteCommand static_sprite(
        std::size_t layer,
        bool atlas = false) {
    nevergone::game_scene_render_queue::SpriteCommand sprite;
    sprite.layer_z_index = layer;
    if (atlas) {
        sprite.resource.kind = nevergone::game_scene_type0_resource::Kind::kSpriteFrameByName;
        sprite.resource.resource_name = "frame.png";
    } else {
        sprite.resource.kind = nevergone::game_scene_type0_resource::Kind::kDirectFile;
        sprite.resource.resource_name = "sprite.png";
        sprite.direct_asset_relative_path = "gamescene/sprite.png";
    }
    return sprite;
}

nevergone::game_scene_initial_player_sprite_request::Request ready_request() {
    namespace request = nevergone::game_scene_initial_player_sprite_request;
    request::Request result;
    result.status = request::Status::kReady;
    result.frame_request.status = nevergone::game_scene_player_frame_request::Status::kReady;
    result.frame_request.position.x = 568.0f;
    result.frame_request.position.y = 320.0f;
    nevergone::game_scene_type0_resource::Selection resource;
    resource.kind = nevergone::game_scene_type0_resource::Kind::kSpriteFrameByName;
    resource.resource_name = "player_idle_001.png";
    result.frame_request.resource = resource;

    nevergone::me_actions_initial_sprite_transform::Plan transform;
    transform.position_x = 10.0f;
    transform.position_y = 20.0f;
    transform.anchor_x = 0.25f;
    transform.anchor_y = 0.75f;
    transform.scale_x = 1.0f;
    transform.scale_y = 1.0f;
    transform.rotation = 0.0f;
    transform.flip_x = false;
    result.sprite_transform = transform;
    return result;
}

}  // namespace

int main() {
    namespace geometry = nevergone::game_scene_initial_player_geometry;

    auto request = ready_request();
    nevergone::game_scene_direct_geometry::Quad quad;
    assert(geometry::build_quad(request, 100, 80, 1136, 640, &quad));

    // Parent point (568,320) + body local point (10,20) = (578,340).
    // Anchor (0.25,0.75) on a 100x80 source-size texture yields local bounds
    // left=-25, right=75, bottom=-60, top=20.
    assert(near(quad.positions[0], -0.02640845f)); // top-left x: 553
    assert(near(quad.positions[1], 0.125f));       // top-left y: 360
    assert(near(quad.positions[2], -0.02640845f));
    assert(near(quad.positions[3], -0.125f));      // bottom-left y: 280
    assert(near(quad.positions[4], 0.14964789f));  // top-right x: 653
    assert(near(quad.positions[5], 0.125f));
    assert(quad.tex_coords[0] == 0.0f && quad.tex_coords[4] == 1.0f);

    request.sprite_transform->flip_x = true;
    assert(geometry::build_quad(request, 100, 80, 1136, 640, &quad));
    assert(quad.tex_coords[0] == 1.0f && quad.tex_coords[4] == 0.0f);

    // Cocos positive rotation is clockwise. With a centered 100x80 texture,
    // +90 rotates top-left (-50,+40) to (+40,+50).
    request.sprite_transform->flip_x = false;
    request.sprite_transform->anchor_x = 0.5f;
    request.sprite_transform->anchor_y = 0.5f;
    request.sprite_transform->position_x = 0.0f;
    request.sprite_transform->position_y = 0.0f;
    request.sprite_transform->rotation = 90.0f;
    assert(geometry::build_quad(request, 100, 80, 1920, 1080, &quad));
    assert(near(quad.positions[0], 80.0f / 1136.0f));
    assert(near(quad.positions[1], 100.0f / 640.0f));

    // ExactFit makes clip-space design fractions independent of physical aspect.
    nevergone::game_scene_direct_geometry::Quad square_quad;
    assert(geometry::build_quad(request, 100, 80, 1000, 1000, &square_quad));
    for (std::size_t i = 0; i < quad.positions.size(); ++i) {
        assert(near(quad.positions[i], square_quad.positions[i]));
    }

    request.status = nevergone::game_scene_initial_player_sprite_request::Status::kFrameRequestFailed;
    assert(!geometry::build_quad(request, 100, 80, 1136, 640, &quad));
    request = ready_request();
    request.frame_request.status = nevergone::game_scene_player_frame_request::Status::kInvalidFrameKey;
    assert(!geometry::build_quad(request, 100, 80, 1136, 640, &quad));
    request = ready_request();
    request.frame_request.resource.reset();
    assert(!geometry::build_quad(request, 100, 80, 1136, 640, &quad));
    request = ready_request();
    request.sprite_transform.reset();
    assert(!geometry::build_quad(request, 100, 80, 1136, 640, &quad));
    request = ready_request();
    request.frame_request.position.x = std::numeric_limits<float>::quiet_NaN();
    assert(!geometry::build_quad(request, 100, 80, 1136, 640, &quad));
    request = ready_request();
    request.sprite_transform->anchor_y = std::numeric_limits<float>::infinity();
    assert(!geometry::build_quad(request, 100, 80, 1136, 640, &quad));
    request = ready_request();
    assert(!geometry::build_quad(request, 0, 80, 1136, 640, &quad));
    assert(!geometry::build_quad(request, 100, 80, 0, 640, &quad));
    assert(!geometry::build_quad(request, 100, 80, 1136, 640, nullptr));

    nevergone::game_scene_render_queue::Queue queue;
    queue.sprites.push_back(static_sprite(0u));
    queue.sprites.push_back(static_sprite(7u, true));
    queue.sprites.push_back(static_sprite(7u));
    queue.sprites.push_back(static_sprite(8u));
    queue.sprites.push_back(static_sprite(9u, true));
    assert(geometry::static_draw_count_before_player(queue) == 3u);

    // A malformed/non-drawable static command does not consume a draw slot.
    queue.sprites[1].resource.resource_name.clear();
    assert(geometry::static_draw_count_before_player(queue) == 2u);

    return 0;
}
