#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "game_scene_construction_plan.h"
#include "game_scene_render_queue.h"

namespace nevergone::game_scene_direct_geometry {

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

// Return renderer-facing sprite command indexes for direct-file sprites in the
// effective Cocos draw order: layer array order first, then child local Z. For
// equal local Z values, preserve the original insertion/object order.
std::vector<std::size_t> ordered_direct_sprite_indices(
        const game_scene_render_queue::Queue& queue);

}  // namespace nevergone::game_scene_direct_geometry
