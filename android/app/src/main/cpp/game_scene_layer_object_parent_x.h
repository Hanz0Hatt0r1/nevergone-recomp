#pragma once

#include <cmath>
#include <cstddef>
#include <optional>

#include "game_scene_construction_plan.h"

namespace nevergone::game_scene_layer_object_parent_x {

// GameScene::loadingTex() creates the eleven gameplay GameSceneClassLayer
// instances with IDs 1..11 while the project construction plan retains them as
// z-index slots 0..10.
constexpr std::size_t kGameplayLayerSlotCount =
        game_scene_construction_plan::kLoadingTexLayerSlotCount;
constexpr std::size_t kMode8LayerZIndex = 7u;

// Recover the X component passed by GameSceneClassLayer::move() to every
// GameSceneObject in one gameplay layer. Native mode +0x12c is createWithID()
// input, therefore layer_id = layer_z_index + 1. Mode 8 forces parent X to zero.
// Every other gameplay mode computes X as (1 - layer_factor) * move_input.x;
// modes 9..11 have additional Y behavior but do not alter this X formula.
inline std::optional<float> from_move_input(
        std::size_t layer_z_index,
        float layer_camera_factor,
        float move_input_x) {
    if (layer_z_index >= kGameplayLayerSlotCount ||
            !std::isfinite(layer_camera_factor) ||
            !std::isfinite(move_input_x)) {
        return std::nullopt;
    }
    if (layer_z_index == kMode8LayerZIndex) return 0.0f;
    const float value = (1.0f - layer_camera_factor) * move_input_x;
    if (!std::isfinite(value)) return std::nullopt;
    return value;
}

}  // namespace nevergone::game_scene_layer_object_parent_x
