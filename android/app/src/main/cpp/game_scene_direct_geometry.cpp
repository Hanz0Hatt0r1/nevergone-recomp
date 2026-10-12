#include "game_scene_direct_geometry.h"

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

bool build_quad_with_local_center(
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        int texture_width,
        int texture_height,
        float local_center_x,
        float local_center_y,
        int surface_width,
        int surface_height,
        Quad* out) {
    if (out == nullptr) return false;
    *out = {};
    if (texture_width <= 0 || texture_height <= 0 ||
        surface_width <= 0 || surface_height <= 0 ||
        !finite_transform(transform) ||
        !std::isfinite(local_center_x) || !std::isfinite(local_center_y)) {
        return false;
    }

    const float half_w = static_cast<float>(texture_width) * 0.5f;
    const float half_h = static_cast<float>(texture_height) * 0.5f;
    const float local_x[4] = {
        local_center_x - half_w,
        local_center_x - half_w,
        local_center_x + half_w,
        local_center_x + half_w,
    };
    const float local_y[4] = {
        local_center_y + half_h,
        local_center_y - half_h,
        local_center_y + half_h,
        local_center_y - half_h,
    };

    // Cocos CCNode::nodeToParentTransform() multiplies positive rotation by
    // -pi/180. Reproduce that clockwise convention here.
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

        // ResolutionPolicy 0 in the shipped CCEGLViewProtocol path is
        // ExactFit: X and Y are scaled independently to the physical viewport.
        // Expressing the design point directly in clip space is therefore
        // independent of the surface aspect ratio.
        out->positions[index * 2u] = 2.0f * design_x / kDesignWidth - 1.0f;
        out->positions[index * 2u + 1u] = 2.0f * design_y / kDesignHeight - 1.0f;
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

}  // namespace

bool build_quad(
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        int texture_width,
        int texture_height,
        int surface_width,
        int surface_height,
        Quad* out) {
    return build_quad_with_local_center(
            transform,
            texture_width,
            texture_height,
            0.0f,
            0.0f,
            surface_width,
            surface_height,
            out);
}

bool build_frame_quad(
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        int texture_width,
        int texture_height,
        int frame_offset_x,
        int frame_offset_y,
        int surface_width,
        int surface_height,
        Quad* out) {
    // A full untrimmed bitmap mirrored around the sprite center also mirrors the
    // transparent padding. Once only the trimmed frame is retained, reproduce
    // that same placement by negating the CCSpriteFrame X offset on flipX.
    const float local_center_x = transform.flip_x
            ? -static_cast<float>(frame_offset_x)
            : static_cast<float>(frame_offset_x);
    return build_quad_with_local_center(
            transform,
            texture_width,
            texture_height,
            local_center_x,
            static_cast<float>(frame_offset_y),
            surface_width,
            surface_height,
            out);
}

bool build_player_quad(
        const game_scene_construction_plan::Type0SpriteTransform& player_node,
        const me_actions_initial_sprite_transform::Plan& body,
        int texture_width,
        int texture_height,
        int surface_width,
        int surface_height,
        Quad* out) {
    if (out == nullptr) return false;
    *out = {};
    if (texture_width <= 0 || texture_height <= 0 ||
        surface_width <= 0 || surface_height <= 0 ||
        !std::isfinite(player_node.position_x) ||
        !std::isfinite(player_node.position_y) ||
        !std::isfinite(body.position_x) || !std::isfinite(body.position_y) ||
        !std::isfinite(body.anchor_x) || !std::isfinite(body.anchor_y) ||
        !std::isfinite(body.scale_x) || !std::isfinite(body.scale_y) ||
        !std::isfinite(body.rotation)) return false;

    const float width = static_cast<float>(texture_width);
    const float height = static_cast<float>(texture_height);
    const float x0 = -body.anchor_x * width;
    const float x1 = (1.0f - body.anchor_x) * width;
    const float y0 = (1.0f - body.anchor_y) * height;
    const float y1 = -body.anchor_y * height;
    const float local_x[4] = {x0, x0, x1, x1};
    const float local_y[4] = {y0, y1, y0, y1};
    const float radians = -body.rotation * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    for (std::size_t index = 0; index < 4u; ++index) {
        const float sx = local_x[index] * body.scale_x;
        const float sy = local_y[index] * body.scale_y;
        const float design_x = player_node.position_x + body.position_x +
            sx * cosine - sy * sine;
        const float design_y = player_node.position_y + body.position_y +
            sx * sine + sy * cosine;
        if (!std::isfinite(design_x) || !std::isfinite(design_y)) return false;
        out->positions[index * 2u] = 2.0f * design_x / kDesignWidth - 1.0f;
        out->positions[index * 2u + 1u] = 2.0f * design_y / kDesignHeight - 1.0f;
    }
    const float left_u = body.flip_x ? 1.0f : 0.0f;
    const float right_u = body.flip_x ? 0.0f : 1.0f;
    out->tex_coords = {left_u, 0.0f, left_u, 1.0f,
                       right_u, 0.0f, right_u, 1.0f};
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
    return indexes;
}

std::vector<std::size_t> ordered_texture_sprite_indices(
        const game_scene_render_queue::Queue& queue) {
    std::vector<std::size_t> indexes;
    indexes.reserve(queue.direct_file_count + queue.sprite_frame_lookup_count);
    for (std::size_t index = 0; index < queue.sprites.size(); ++index) {
        const auto& sprite = queue.sprites[index];
        if (sprite.resource.kind == game_scene_type0_resource::Kind::kDirectFile &&
            sprite.direct_asset_relative_path.has_value()) {
            indexes.push_back(index);
            continue;
        }
        if (sprite.resource.kind == game_scene_type0_resource::Kind::kSpriteFrameByName &&
            !sprite.resource.resource_name.empty()) {
            indexes.push_back(index);
        }
    }
    return indexes;
}

}  // namespace nevergone::game_scene_direct_geometry
