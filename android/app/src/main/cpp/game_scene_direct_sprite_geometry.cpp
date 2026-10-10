#include "game_scene_direct_sprite_geometry.h"

#include <cmath>

namespace nevergone::game_scene_direct_sprite_geometry {
namespace {

constexpr float kPi = 3.14159265358979323846f;

void transform_corner(
        float local_x,
        float local_y,
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        float* clip_x,
        float* clip_y) {
    // CCNode::nodeToParentTransform() multiplies rotation degrees by
    // -pi/180, so positive Cocos rotation is clockwise in design space.
    const float radians = -transform.rotation * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float scaled_x = local_x * transform.scale_x;
    const float scaled_y = local_y * transform.scale_y;
    const float design_x = transform.position_x +
            cosine * scaled_x - sine * scaled_y;
    const float design_y = transform.position_y +
            sine * scaled_x + cosine * scaled_y;
    *clip_x = (2.0f * design_x / kDesignWidth) - 1.0f;
    *clip_y = (2.0f * design_y / kDesignHeight) - 1.0f;
}

}  // namespace

bool build(
        const game_scene_render_queue::SpriteCommand& command,
        int texture_width,
        int texture_height,
        Quad* out) {
    if (out == nullptr || texture_width <= 0 || texture_height <= 0 ||
        !command.direct_asset_relative_path.has_value()) {
        if (out != nullptr) *out = {};
        return false;
    }

    const float half_width = static_cast<float>(texture_width) * 0.5f;
    const float half_height = static_cast<float>(texture_height) * 0.5f;
    const float left_u = command.transform.flip_x ? 1.0f : 0.0f;
    const float right_u = command.transform.flip_x ? 0.0f : 1.0f;

    const float local_x[] = {-half_width, -half_width, half_width, half_width};
    const float local_y[] = {half_height, -half_height, half_height, -half_height};
    const float u[] = {left_u, left_u, right_u, right_u};
    // Bitmap.getPixels() emits the top row first. The GLES upload keeps that
    // row at texture v=0, so top vertices deliberately sample v=0.
    const float v[] = {0.0f, 1.0f, 0.0f, 1.0f};

    Quad quad;
    for (std::size_t index = 0; index < quad.vertices.size(); ++index) {
        transform_corner(
                local_x[index],
                local_y[index],
                command.transform,
                &quad.vertices[index].x,
                &quad.vertices[index].y);
        quad.vertices[index].u = u[index];
        quad.vertices[index].v = v[index];
    }
    *out = quad;
    return true;
}

}  // namespace nevergone::game_scene_direct_sprite_geometry
