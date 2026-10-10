#include <cassert>
#include <cmath>
#include <cstddef>
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

    // 16:9 design geometry remains centered under aspect-fit letterboxing.
    transform.scale_x = 1.0f;
    transform.scale_y = 1.0f;
    assert(geometry::build_quad(transform, 100, 50, 1000, 1000, &quad));
    const float center_x = (quad.positions[0] + quad.positions[4]) * 0.5f;
    const float center_y = (quad.positions[1] + quad.positions[3]) * 0.5f;
    assert(near(center_x, 0.0f));
    assert(near(center_y, 0.0f));

    assert(!geometry::build_quad(transform, 0, 50, 1136, 640, &quad));
    assert(!geometry::build_quad(transform, 100, 50, 0, 640, &quad));
    assert(!geometry::build_quad(transform, 100, 50, 1136, 640, nullptr));

    nevergone::game_scene_render_queue::Queue queue;
    queue.sprites.push_back(direct_sprite(1u, 5));   // index 0
    queue.sprites.push_back(direct_sprite(0u, 10));  // index 1
    auto frame = direct_sprite(0u, -5);
    frame.resource.kind = nevergone::game_scene_type0_resource::Kind::kSpriteFrameByName;
    frame.direct_asset_relative_path.reset();
    queue.sprites.push_back(frame);                   // index 2, excluded
    queue.sprites.push_back(direct_sprite(0u, -2));  // index 3
    queue.sprites.push_back(direct_sprite(0u, -2));  // index 4
    queue.direct_file_count = 4u;

    const std::vector<std::size_t> order = geometry::ordered_direct_sprite_indices(queue);
    assert((order == std::vector<std::size_t>{3u, 4u, 1u, 0u}));

    return 0;
}
