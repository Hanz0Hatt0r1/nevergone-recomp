#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

namespace nevergone::game_scene_camera_contract {

constexpr float kDesignWidth = 1136.0f;
constexpr float kCameraLeadFraction = 0.12f;
constexpr std::size_t kFixedClassLayerSourceIndex = 7u;

// GameScene::loadingTex() starts horizontal camera displacement once the
// matching type-6 entry marker passes 0.5 + 0.12 of the Cocos win width. The
// displacement is capped by GameScenePortNodeData +0x4b4 minus half the win
// width, then clamped to zero.
inline std::optional<float> initial_eye_x(
        float marker_source_x,
        float scene_extent_x,
        float design_width = kDesignWidth) {
    if (!std::isfinite(marker_source_x) || !std::isfinite(scene_extent_x) ||
        !std::isfinite(design_width) || design_width <= 0.0f) {
        return std::nullopt;
    }

    const float threshold = design_width * (0.5f + kCameraLeadFraction);
    const float maximum = scene_extent_x - design_width * 0.5f;
    float displacement = marker_source_x - threshold;
    if (displacement > maximum) displacement = maximum;
    if (displacement < 0.0f) displacement = 0.0f;
    return displacement;
}

// updateCameraTrack() forwards cameraEyeX to every recovered class layer.
// GameSceneClassLayer ID 8 (source layer index 7) pins its object-parent X to
// zero. Every other class layer moves each GameSceneObject parent by
// (1-factor)*cameraEyeX. The GameScene camera itself shifts the complete world
// by -cameraEyeX, so the flattened renderer needs the net translations below.
inline std::optional<float> static_screen_translation_x(
        std::size_t source_layer_index,
        float layer_camera_factor,
        float camera_eye_x) {
    if (!std::isfinite(layer_camera_factor) || !std::isfinite(camera_eye_x)) {
        return std::nullopt;
    }
    if (source_layer_index == kFixedClassLayerSourceIndex) {
        return -camera_eye_x;
    }
    return -layer_camera_factor * camera_eye_x;
}

inline std::optional<float> world_screen_translation_x(float camera_eye_x) {
    if (!std::isfinite(camera_eye_x)) return std::nullopt;
    return -camera_eye_x;
}

}  // namespace nevergone::game_scene_camera_contract
