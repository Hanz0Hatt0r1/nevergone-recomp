#include <algorithm>
#include <cassert>
#include <cmath>

#include "game_scene_direct_geometry.h"
#include "scene_actions_sprite_bounds.h"

namespace {

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

nevergone::scene_actions_spatial_trigger::Rect rect_from_quad(
        const nevergone::game_scene_direct_geometry::Quad& quad) {
    using namespace nevergone::game_scene_direct_geometry;
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    for (std::size_t index = 0; index < 4u; ++index) {
        const float x = (quad.positions[index * 2u] + 1.0f) * 0.5f * kDesignWidth;
        const float y = (quad.positions[index * 2u + 1u] + 1.0f) * 0.5f * kDesignHeight;
        if (index == 0u) {
            min_x = max_x = x;
            min_y = max_y = y;
        } else {
            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
        }
    }
    return {min_x, min_y, max_x - min_x, max_y - min_y};
}

void compare(
        const nevergone::game_scene_construction_plan::Type0SpriteTransform& transform,
        float skew,
        int width,
        int height) {
    nevergone::game_scene_direct_geometry::Quad quad;
    assert(nevergone::game_scene_direct_geometry::build_quad_with_skew_x(
            transform,
            skew,
            width,
            height,
            static_cast<int>(nevergone::game_scene_direct_geometry::kDesignWidth),
            static_cast<int>(nevergone::game_scene_direct_geometry::kDesignHeight),
            &quad));
    const auto expected = rect_from_quad(quad);
    const auto actual = nevergone::scene_actions_sprite_bounds::build_untrimmed(
            transform, skew, width, height);
    assert(actual.has_value());
    assert(near(actual->x, expected.x));
    assert(near(actual->y, expected.y));
    assert(near(actual->width, expected.width));
    assert(near(actual->height, expected.height));
}

}  // namespace

int main() {
    nevergone::game_scene_construction_plan::Type0SpriteTransform transform;
    transform.position_x = 300.0f;
    transform.position_y = 200.0f;
    transform.rotation = 0.0f;
    transform.scale_x = 1.0f;
    transform.scale_y = 1.0f;
    compare(transform, 0.0f, 20, 40);

    transform.rotation = 31.0f;
    transform.scale_x = 1.25f;
    transform.scale_y = 0.75f;
    compare(transform, 0.0f, 37, 53);

    transform.rotation = -18.0f;
    transform.scale_x = -1.1f;
    transform.scale_y = 1.4f;
    transform.flip_x = true;  // flip changes texcoords, not node-space AABB.
    compare(transform, 12.5f, 64, 28);

    assert(!nevergone::scene_actions_sprite_bounds::build_untrimmed(
            transform, 0.0f, 0, 20).has_value());
    return 0;
}
