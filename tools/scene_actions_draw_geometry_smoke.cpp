#include <cassert>
#include <cmath>

#include "scene_actions_draw_geometry.h"

namespace draw = nevergone::scene_actions_draw_geometry;
namespace render = nevergone::scene_actions_render_frame_plan;

static bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

int main() {
    render::Plan frame{};
    frame.width = 100;
    frame.height = 50;
    frame.geometry.position_x = 10.0f;
    frame.geometry.position_y = 20.0f;
    frame.geometry.anchor_x = 0.5f;
    frame.geometry.anchor_y = 0.5f;
    frame.geometry.rotation = 0.0f;
    frame.geometry.scale_x = 1.0f;
    frame.geometry.scale_y = 1.0f;

    const auto quad = draw::build(frame, 100.0f, 200.0f, 1136.0f, 640.0f);
    assert(quad.has_value());
    const float left = (60.0f / 1136.0f) * 2.0f - 1.0f;
    const float bottom = (195.0f / 640.0f) * 2.0f - 1.0f;
    assert(near(quad->positions[0], left));
    assert(near(quad->positions[1], bottom));
    assert(quad->texcoords[0] == 0.0f);

    frame.geometry.flip_x = true;
    const auto flipped = draw::build(frame, 100.0f, 200.0f, 1136.0f, 640.0f);
    assert(flipped.has_value());
    assert(flipped->texcoords[0] == 1.0f);
    assert(flipped->texcoords[4] == 0.0f);

    frame.geometry.rotation = 90.0f;
    const auto rotated = draw::build(frame, 0.0f, 0.0f, 1136.0f, 640.0f);
    assert(rotated.has_value());
    assert(std::isfinite(rotated->positions[0]));

    frame.width = 0;
    assert(!draw::build(frame, 0.0f, 0.0f, 1136.0f, 640.0f).has_value());
    return 0;
}
