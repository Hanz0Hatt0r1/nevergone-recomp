#include "game_scene_initial_player_geometry.h"

#include <cmath>

namespace nevergone::game_scene_initial_player_geometry {
namespace {

constexpr float kPi = 3.14159265358979323846f;

bool finite_request(
        const game_scene_initial_player_sprite_request::Request& request) {
    if (request.status != game_scene_initial_player_sprite_request::Status::kReady ||
            !request.sprite_transform.has_value()) {
        return false;
    }
    const auto& position = request.frame_request.position;
    const auto& transform = *request.sprite_transform;
    return std::isfinite(position.x) && std::isfinite(position.y) &&
            std::isfinite(transform.position_x) &&
            std::isfinite(transform.position_y) &&
            std::isfinite(transform.anchor_x) &&
            std::isfinite(transform.anchor_y) &&
            std::isfinite(transform.scale_x) &&
            std::isfinite(transform.scale_y) &&
            std::isfinite(transform.rotation);
}

bool drawable_static_sprite(const game_scene_render_queue::SpriteCommand& sprite) {
    if (sprite.resource.kind == game_scene_type0_resource::Kind::kDirectFile) {
        return sprite.direct_asset_relative_path.has_value();
    }
    return sprite.resource.kind == game_scene_type0_resource::Kind::kSpriteFrameByName &&
            !sprite.resource.resource_name.empty();
}

}  // namespace

bool build_quad(
        const game_scene_initial_player_sprite_request::Request& request,
        int texture_width,
        int texture_height,
        int surface_width,
        int surface_height,
        game_scene_direct_geometry::Quad* out) {
    if (out == nullptr) return false;
    *out = {};
    if (texture_width <= 0 || texture_height <= 0 ||
            surface_width <= 0 || surface_height <= 0 ||
            !finite_request(request)) {
        return false;
    }

    const auto& player = request.frame_request.position;
    const auto& transform = *request.sprite_transform;
    const float width = static_cast<float>(texture_width);
    const float height = static_cast<float>(texture_height);

    // Java reconstructs TexturePacker frames as standalone transparent images
    // at spriteSourceSize. Anchor-point geometry can therefore operate on the
    // full source-size texture exactly like the original CCSprite content size.
    const float left = -transform.anchor_x * width;
    const float right = (1.0f - transform.anchor_x) * width;
    const float bottom = -transform.anchor_y * height;
    const float top = (1.0f - transform.anchor_y) * height;
    const float local_x[4] = {left, left, right, right};
    const float local_y[4] = {top, bottom, top, bottom};

    // Match CCNode::nodeToParentTransform(): positive Cocos rotation is
    // clockwise in this renderer's design-space convention.
    const float radians = -transform.rotation * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    for (std::size_t index = 0; index < 4u; ++index) {
        const float scaled_x = local_x[index] * transform.scale_x;
        const float scaled_y = local_y[index] * transform.scale_y;
        const float rotated_x = scaled_x * cosine - scaled_y * sine;
        const float rotated_y = scaled_x * sine + scaled_y * cosine;

        // The body sprite is a child of MeActionsSystem. Its local frame
        // position is applied before the already recovered MEPlayer world point.
        const float design_x =
                player.x + transform.position_x + rotated_x;
        const float design_y =
                player.y + transform.position_y + rotated_y;
        if (!std::isfinite(design_x) || !std::isfinite(design_y)) return false;

        out->positions[index * 2u] =
                2.0f * design_x / game_scene_direct_geometry::kDesignWidth - 1.0f;
        out->positions[index * 2u + 1u] =
                2.0f * design_y / game_scene_direct_geometry::kDesignHeight - 1.0f;
    }

    const float left_u = transform.flip_x ? 1.0f : 0.0f;
    const float right_u = transform.flip_x ? 0.0f : 1.0f;
    out->tex_coords = {
        left_u, 0.0f,
        left_u, 1.0f,
        right_u, 0.0f,
        right_u, 1.0f,
    };
    return true;
}

std::size_t static_draw_count_before_player(
        const game_scene_render_queue::Queue& queue) {
    std::size_t count = 0;
    for (const auto& sprite : queue.sprites) {
        if (!drawable_static_sprite(sprite)) continue;
        if (sprite.layer_z_index > kBattleManagerGameSceneZ) break;
        ++count;
    }
    return count;
}

}  // namespace nevergone::game_scene_initial_player_geometry
