#pragma once

#include <array>
#include <cmath>
#include <optional>

#include "scene_actions_render_frame_plan.h"

namespace nevergone::scene_actions_draw_geometry {

struct Quad {
    std::array<float, 8> positions{};
    std::array<float, 8> texcoords{};
};

inline std::optional<Quad> build(
        const scene_actions_render_frame_plan::Plan& frame,
        float parent_x,
        float parent_y,
        float design_width,
        float design_height) {
    if (!(design_width > 0.0f) || !(design_height > 0.0f) ||
            !std::isfinite(parent_x) || !std::isfinite(parent_y) ||
            frame.width <= 0 || frame.height <= 0 ||
            !std::isfinite(frame.geometry.position_x) ||
            !std::isfinite(frame.geometry.position_y) ||
            !std::isfinite(frame.geometry.anchor_x) ||
            !std::isfinite(frame.geometry.anchor_y) ||
            !std::isfinite(frame.geometry.rotation) ||
            !std::isfinite(frame.geometry.scale_x) ||
            !std::isfinite(frame.geometry.scale_y)) {
        return std::nullopt;
    }

    const float w = static_cast<float>(frame.width) * frame.geometry.scale_x;
    const float h = static_cast<float>(frame.height) * frame.geometry.scale_y;
    if (!std::isfinite(w) || !std::isfinite(h)) return std::nullopt;

    const float left = -frame.geometry.anchor_x * w;
    const float right = left + w;
    const float bottom = -frame.geometry.anchor_y * h;
    const float top = bottom + h;
    const float origin_x = parent_x + frame.geometry.position_x;
    const float origin_y = parent_y + frame.geometry.position_y;
    const float radians = frame.geometry.rotation * 0.01745329251994329577f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    const std::array<float, 8> local = {
        left, bottom,
        left, top,
        right, bottom,
        right, top,
    };

    Quad quad;
    for (std::size_t i = 0; i < 4; ++i) {
        const float x = local[i * 2u + 0u];
        const float y = local[i * 2u + 1u];
        const float world_x = origin_x + x * c - y * s;
        const float world_y = origin_y + x * s + y * c;
        quad.positions[i * 2u + 0u] = world_x / design_width * 2.0f - 1.0f;
        quad.positions[i * 2u + 1u] = world_y / design_height * 2.0f - 1.0f;
    }

    if (!frame.geometry.flip_x) {
        quad.texcoords = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f};
    } else {
        quad.texcoords = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    }
    return quad;
}

}  // namespace nevergone::scene_actions_draw_geometry
