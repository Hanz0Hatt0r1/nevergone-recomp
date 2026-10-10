#include <cassert>
#include <cmath>
#include <string>

#include "game_scene_direct_sprite_geometry.h"

namespace {

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

nevergone::game_scene_render_queue::SpriteCommand command() {
    nevergone::game_scene_render_queue::SpriteCommand value;
    value.direct_asset_relative_path = "gamescene/gs_res_image_file/test.png";
    value.transform.position_x = 568.0f;
    value.transform.position_y = 320.0f;
    value.transform.rotation = 0.0f;
    value.transform.scale_x = 1.0f;
    value.transform.scale_y = 1.0f;
    value.transform.flip_x = false;
    return value;
}

}  // namespace

int main() {
    namespace geometry = nevergone::game_scene_direct_sprite_geometry;

    auto sprite = command();
    geometry::Quad quad;
    assert(geometry::build(sprite, 100, 40, &quad));

    // Centered 100x40 sprite at the exact 1136x640 design center.
    assert(near(quad.vertices[0].x, -50.0f / 568.0f));
    assert(near(quad.vertices[0].y, 20.0f / 320.0f));
    assert(near(quad.vertices[1].x, -50.0f / 568.0f));
    assert(near(quad.vertices[1].y, -20.0f / 320.0f));
    assert(near(quad.vertices[2].x, 50.0f / 568.0f));
    assert(near(quad.vertices[2].y, 20.0f / 320.0f));
    assert(near(quad.vertices[3].x, 50.0f / 568.0f));
    assert(near(quad.vertices[3].y, -20.0f / 320.0f));
    assert(near(quad.vertices[0].u, 0.0f));
    assert(near(quad.vertices[0].v, 0.0f));
    assert(near(quad.vertices[1].v, 1.0f));

    // Positive Cocos rotation is clockwise. Top-left (-50,+20) rotated +90
    // therefore becomes (+20,+50) relative to the sprite center.
    sprite.transform.rotation = 90.0f;
    assert(geometry::build(sprite, 100, 40, &quad));
    assert(near(quad.vertices[0].x, 20.0f / 568.0f));
    assert(near(quad.vertices[0].y, 50.0f / 320.0f));

    // Scale applies in local sprite space before rotation.
    sprite.transform.rotation = 0.0f;
    sprite.transform.scale_x = 2.0f;
    sprite.transform.scale_y = 0.5f;
    assert(geometry::build(sprite, 100, 40, &quad));
    assert(near(quad.vertices[0].x, -100.0f / 568.0f));
    assert(near(quad.vertices[0].y, 10.0f / 320.0f));

    // setFlipX changes texture coordinates, not geometry.
    const float before_x = quad.vertices[0].x;
    const float before_y = quad.vertices[0].y;
    sprite.transform.flip_x = true;
    assert(geometry::build(sprite, 100, 40, &quad));
    assert(near(quad.vertices[0].x, before_x));
    assert(near(quad.vertices[0].y, before_y));
    assert(near(quad.vertices[0].u, 1.0f));
    assert(near(quad.vertices[2].u, 0.0f));

    // ExactFit design-space corners map to clip-space corners for a zero-size
    // conceptual point at the same position.
    sprite.transform.scale_x = 0.0f;
    sprite.transform.scale_y = 0.0f;
    sprite.transform.position_x = 0.0f;
    sprite.transform.position_y = 0.0f;
    assert(geometry::build(sprite, 1, 1, &quad));
    assert(near(quad.vertices[0].x, -1.0f));
    assert(near(quad.vertices[0].y, -1.0f));
    sprite.transform.position_x = geometry::kDesignWidth;
    sprite.transform.position_y = geometry::kDesignHeight;
    assert(geometry::build(sprite, 1, 1, &quad));
    assert(near(quad.vertices[0].x, 1.0f));
    assert(near(quad.vertices[0].y, 1.0f));

    nevergone::game_scene_render_queue::SpriteCommand unresolved;
    assert(!geometry::build(unresolved, 100, 40, &quad));
    assert(!geometry::build(sprite, 0, 40, &quad));
    assert(!geometry::build(sprite, 100, 40, nullptr));
    return 0;
}
