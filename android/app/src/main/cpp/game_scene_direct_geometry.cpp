#include "game_scene_direct_geometry.h"

#include <algorithm>
#include <cmath>

#include "game_scene_type0_resource.h"

namespace nevergone::game_scene_direct_geometry {
namespace {

constexpr float kPi = 3.14159265358979323846f;

bool finite_transform(const game_scene_construction_plan::Type0SpriteTransform& transform) {
    return std::isfinite(transform.position_x) &&
        std::isfinite(transform.position_y) &&
        std::isfinite(transform.rotation) &&
        std::isfinite(transform.scale_x) &&
        std::isfinite(transform.scale_y);
}

}  // namespace

bool build_quad(
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        int texture_width,
        int texture_height,
        int surface_width,
        int surface_height,
        Quad* out) {
    if (out == nullptr) return false;
    *out = {};
    if (texture_width <= 0 || texture_height <= 0 ||
        surface_width <= 0 || surface_height <= 0 ||
        !finite_transform(transform)) {
        return false;
    }

    const float surface_w = static_cast<float>(surface_width);
    const float surface_h = static_cast<float>(surface_height);
    const float fit_scale = std::min(surface_w / kDesignWidth, surface_h / kDesignHeight);
    if (!std::isfinite(fit_scale) || fit_scale <= 0.0f) return false;
    const float viewport_w = kDesignWidth * fit_scale;
    const float viewport_h = kDesignHeight * fit_scale;
    const float offset_x = (surface_w - viewport_w) * 0.5f;
    const float offset_y = (surface_h - viewport_h) * 0.5f;

    const float half_w = static_cast<float>(texture_width) * 0.5f;
    const float half_h = static_cast<float>(texture_height) * 0.5f;
    const float local_x[4] = {-half_w, -half_w, half_w, half_w};
    const float local_y[4] = {half_h, -half_h, half_h, -half_h};

    // Cocos2d-x CCNode rotation uses positive values clockwise. Standard 2D
    // matrix rotation is counter-clockwise, so apply the negative angle here.
    const float radians = -transform.rotation * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    for (std::size_t index = 0; index < 4u; ++index) {
        const float scaled_x = local_x[index] * transform.scale_x;
        const float scaled_y = local_y[index] * transform.scale_y;
        const float rotated_x = scaled_x * cosine - scaled_y * sine;
        const float rotated_y = scaled_x * sine + scaled_y * cosine;
        const float design_x = transform.position_x + rotated_x;
        const float design_y = transform.position_y + rotated_y;
        const float pixel_x = offset_x + design_x * fit_scale;
        const float pixel_y = offset_y + design_y * fit_scale;
        out->positions[index * 2u] = 2.0f * pixel_x / surface_w - 1.0f;
        out->positions[index * 2u + 1u] = 2.0f * pixel_y / surface_h - 1.0f;
    }

    const float left_u = transform.flip_x ? 1.0f : 0.0f;
    const float right_u = transform.flip_x ? 0.0f : 1.0f;
    // Android Bitmap rows are uploaded top-to-bottom. Match the existing
    // imported-texture convention by mapping v=0 to the visual top edge.
    out->tex_coords = {
        left_u, 0.0f,
        left_u, 1.0f,
        right_u, 0.0f,
        right_u, 1.0f,
    };
    return true;
}

std::vector<std::size_t> ordered_direct_sprite_indices(
        const game_scene_render_queue::Queue& queue) {
    std::vector<std::size_t> indexes;
    indexes.reserve(queue.direct_file_count);
    for (std::size_t index = 0; index < queue.sprites.size(); ++index) {
        const auto& sprite = queue.sprites[index];
        if (sprite.resource.kind == game_scene_type0_resource::Kind::kDirectFile &&
            sprite.direct_asset_relative_path.has_value()) {
            indexes.push_back(index);
        }
    }

    std::stable_sort(indexes.begin(), indexes.end(), [&](std::size_t lhs, std::size_t rhs) {
        const auto& a = queue.sprites[lhs];
        const auto& b = queue.sprites[rhs];
        if (a.layer_z_index != b.layer_z_index) return a.layer_z_index < b.layer_z_index;
        return a.transform.child_z_order < b.transform.child_z_order;
    });
    return indexes;
}

}  // namespace nevergone::game_scene_direct_geometry
