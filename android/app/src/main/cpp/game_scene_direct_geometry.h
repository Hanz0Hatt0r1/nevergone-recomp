#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "game_scene_construction_plan.h"
#include "game_scene_render_queue.h"

namespace nevergone::game_scene_direct_geometry {

// Original Never Gone 1.0.9 calls
// CCEGLViewProtocol::setDesignResolutionSize(1136, 640, 0). The policy-0
// implementation uses independent X/Y scales (Cocos2d-x ExactFit), so the
// entire design rectangle maps to the entire GL viewport.
constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;

struct Quad {
    std::array<float, 8> positions{};
    std::array<float, 8> tex_coords{};
};

bool build_quad(
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        int texture_width,
        int texture_height,
        int surface_width,
        int surface_height,
        Quad* out);

// Same type-0 sprite geometry with the recovered CCNode skewX applied before
// scale/rotation. This matches the Cocos2d-x node transform order used by the
// original gkshu01..08 CCSkewTo actions. Zero skew is identical to build_quad().
bool build_quad_with_skew_x(
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        float skew_x_degrees,
        int texture_width,
        int texture_height,
        int surface_width,
        int surface_height,
        Quad* out);

bool build_player_quad(
        const game_scene_construction_plan::Type0SpriteTransform& player_node,
        const me_actions_initial_sprite_transform::Plan& body,
        int texture_width,
        int texture_height,
        int surface_width,
        int surface_height,
        Quad* out);

std::vector<std::size_t> ordered_direct_sprite_indices(
        const game_scene_render_queue::Queue& queue);

std::vector<std::size_t> ordered_texture_sprite_indices(
        const game_scene_render_queue::Queue& queue);

}  // namespace nevergone::game_scene_direct_geometry
