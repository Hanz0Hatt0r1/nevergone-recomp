#pragma once

#include <array>

#include "game_scene_render_queue.h"

namespace nevergone::game_scene_direct_sprite_geometry {

// Original Never Gone 1.0.9 AppDelegate::AddAllSearchPath() calls
// CCEGLViewProtocol::setDesignResolutionSize(1136, 640, 0). Policy 0 follows
// the separate X/Y scale path (ExactFit), so design coordinates map directly
// across the complete GL viewport.
constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;

struct Vertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

struct Quad {
    // Triangle-strip order: top-left, bottom-left, top-right, bottom-right.
    std::array<Vertex, 4> vertices{};
};

// Build clip-space geometry for a proven direct-file sprite command.
// The original engine initializes content scale to 1.0 and uses the default
// CCSprite anchor (0.5, 0.5), so decoded pixel dimensions are also design-unit
// dimensions for this path.
bool build(
        const game_scene_render_queue::SpriteCommand& command,
        int texture_width,
        int texture_height,
        Quad* out);

}  // namespace nevergone::game_scene_direct_sprite_geometry
