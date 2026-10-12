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

// CCSpriteFrame retains its trim offset from the untrimmed content center. A
// standalone trimmed texture must rotate/scale that local offset together with
// the frame pixels instead of centering the trim on the node.
bool build_frame_quad(
        const game_scene_construction_plan::Type0SpriteTransform& transform,
        int texture_width,
        int texture_height,
        int frame_offset_x,
        int frame_offset_y,
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

// Filter the already recovered layer/object traversal order down to direct-file
// sprites. `child_z_order` belongs to the sprite child inside each distinct
// GameSceneObject and must not reorder separate GameSceneObject instances.
std::vector<std::size_t> ordered_direct_sprite_indices(
        const game_scene_render_queue::Queue& queue);

// The static type-0 renderer uses the same traversal order for both recovered
// CCSprite::create(file) resources and spriteFrameByName resources. Atlas-backed
// frames retain trimmed pixels plus their CCSpriteFrame center offset.
std::vector<std::size_t> ordered_texture_sprite_indices(
        const game_scene_render_queue::Queue& queue);

}  // namespace nevergone::game_scene_direct_geometry
