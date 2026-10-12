#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "game_scene_direct_geometry.h"

namespace {

bool near(float actual, float expected, float epsilon = 0.0001f) {
    return std::fabs(actual - expected) <= epsilon;
}

nevergone::game_scene_render_queue::SpriteCommand direct_sprite(
        std::size_t layer,
        std::int32_t child_z) {
    nevergone::game_scene_render_queue::SpriteCommand sprite;
    sprite.layer_z_index = layer;
    sprite.resource.kind = nevergone::game_scene_type0_resource::Kind::kDirectFile;
    sprite.resource.resource_name = "gktianchong.png";
    sprite.direct_asset_relative_path = "gamescene/gs_res_image_file/gktianchong.png";
    sprite.transform.child_z_order = child_z;
    return sprite;
}

}  // namespace

int main() {
    namespace geometry = nevergone::game_scene_direct_geometry;
    using Transform = nevergone::game_scene_construction_plan::Type0SpriteTransform;

    Transform transform;
    transform.position_x = 568.0f;
    transform.position_y = 320.0f;
    transform.scale_x = 1.0f;
    transform.scale_y = 1.0f;

    geometry::Quad quad;
    assert(geometry::build_quad(transform, 100, 50, 1136, 640, &quad));
    assert(near(quad.positions[0], -0.08802817f));
    assert(near(quad.positions[1], 0.078125f));
    assert(near(quad.positions[2], -0.08802817f));
    assert(near(quad.positions[3], -0.078125f));
    assert(near(quad.positions[4], 0.08802817f));
    assert(near(quad.positions[5], 0.078125f));
    assert(near(quad.positions[6], 0.08802817f));
    assert(near(quad.positions[7], -0.078125f));
    assert(quad.tex_coords[0] == 0.0f && quad.tex_coords[1] == 0.0f);
    assert(quad.tex_coords[4] == 1.0f && quad.tex_coords[5] == 0.0f);

    // Zero runtime skew must preserve the established P7 first-frame geometry.
    geometry::Quad zero_skew;
    assert(geometry::build_quad_with_skew_x(
            transform, 0.0f, 100, 50, 1136, 640, &zero_skew));
    for (std::size_t i = 0; i < quad.positions.size(); ++i) {
        assert(near(zero_skew.positions[i], quad.positions[i]));
    }

    // The recovered CCSkewTo calls pass the randomized value in r1 and zero in
    // r2: skewX changes X by tan(skewX) * localY while preserving Y. At +45°
    // the top edge moves +25 design units and the bottom edge -25.
    geometry::Quad skewed;
    assert(geometry::build_quad_with_skew_x(
            transform, 45.0f, 100, 50, 1136, 640, &skewed));
    assert(near(skewed.positions[0], -0.04401408f));
    assert(near(skewed.positions[1], 0.078125f));
    assert(near(skewed.positions[2], -0.13204225f));
    assert(near(skewed.positions[3], -0.078125f));
    assert(near(skewed.positions[4], 0.13204225f));
    assert(near(skewed.positions[5], 0.078125f));
    assert(near(skewed.positions[6], 0.04401408f));
    assert(near(skewed.positions[7], -0.078125f));
    assert(!geometry::build_quad_with_skew_x(
            transform, std::numeric_limits<float>::quiet_NaN(),
            100, 50, 1136, 640, &skewed));

    transform.flip_x = true;
    assert(geometry::build_quad(transform, 100, 50, 1136, 640, &quad));
    assert(quad.tex_coords[0] == 1.0f && quad.tex_coords[1] == 0.0f);
    assert(quad.tex_coords[4] == 0.0f && quad.tex_coords[5] == 0.0f);

    // Cocos positive rotation is clockwise. At +90 degrees the original
    // top-left local corner (-50,+25) becomes (+25,+50).
    transform.flip_x = false;
    transform.rotation = 90.0f;
    assert(geometry::build_quad(transform, 100, 50, 1136, 640, &quad));
    assert(near(quad.positions[0], 0.04401408f));
    assert(near(quad.positions[1], 0.15625f));

    transform.rotation = 0.0f;
    transform.scale_x = 2.0f;
    transform.scale_y = 0.5f;
    assert(geometry::build_quad(transform, 100, 50, 1136, 640, &quad));
    assert(near(quad.positions[0], -0.17605634f));
    assert(near(quad.positions[1], 0.0390625f));

    // The original uses ResolutionPolicy 0 (ExactFit), not letterboxing.
    transform.scale_x = 1.0f;
    transform.scale_y = 1.0f;
    assert(geometry::build_quad(transform, 100, 50, 1000, 1000, &quad));
    assert(near(quad.positions[0], -0.08802817f));
    assert(near(quad.positions[1], 0.078125f));
    assert(near(quad.positions[4], 0.08802817f));
    assert(near(quad.positions[7], -0.078125f));

    assert(!geometry::build_quad(transform, 0, 50, 1136, 640, &quad));
    assert(!geometry::build_quad(transform, 100, 50, 0, 640, &quad));
    assert(!geometry::build_quad(transform, 100, 50, 1136, 640, nullptr));

    Transform player_node;
    player_node.position_x = 600.0f;
    player_node.position_y = 468.0f;
    nevergone::me_actions_initial_sprite_transform::Plan body;
    body.position_x = 12.0f;
    body.position_y = -4.0f;
    body.anchor_x = 0.5f;
    body.anchor_y = 0.25f;
    body.scale_x = 1.0f;
    body.scale_y = 1.0f;
    assert(geometry::build_player_quad(player_node, body, 100, 50, 1136, 640, &quad));
    assert(near(quad.positions[0], 2.0f * 562.0f / 1136.0f - 1.0f));
    assert(near(quad.positions[1], 2.0f * 501.5f / 640.0f - 1.0f));
    body.flip_x = true;
    assert(geometry::build_player_quad(player_node, body, 100, 50, 1136, 640, &quad));
    assert(quad.tex_coords[0] == 1.0f && quad.tex_coords[4] == 0.0f);
    body.scale_x = std::numeric_limits<float>::quiet_NaN();
    assert(!geometry::build_player_quad(player_node, body, 100, 50, 1136, 640, &quad));

    nevergone::game_scene_render_queue::Queue queue;
    queue.sprites.push_back(direct_sprite(1u, 5));
    queue.sprites.push_back(direct_sprite(0u, 10));
    auto frame = direct_sprite(0u, -5);
    frame.resource.kind = nevergone::game_scene_type0_resource::Kind::kSpriteFrameByName;
    frame.resource.resource_name = "atlas-frame.png";
    frame.direct_asset_relative_path.reset();
    queue.sprites.push_back(frame);
    queue.sprites.push_back(direct_sprite(0u, -2));
    queue.sprites.push_back(direct_sprite(0u, -2));
    queue.direct_file_count = 4u;
    queue.sprite_frame_lookup_count = 1u;

    const std::vector<std::size_t> direct_order = geometry::ordered_direct_sprite_indices(queue);
    assert((direct_order == std::vector<std::size_t>{0u, 1u, 3u, 4u}));

    const std::vector<std::size_t> static_order = geometry::ordered_texture_sprite_indices(queue);
    assert((static_order == std::vector<std::size_t>{0u, 1u, 2u, 3u, 4u}));

    return 0;
}
