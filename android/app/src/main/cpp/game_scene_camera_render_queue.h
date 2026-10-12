#pragma once

#include <utility>

#include "game_scene_camera_contract.h"
#include "game_scene_render_queue.h"

namespace nevergone::game_scene_camera_render_queue {

// Applies the flattened equivalent of GameScene camera X plus the per-class-
// layer GameSceneObject parent movement. The input must still be the serialized
// world queue: player/BattleManager and screen-space Controls UI are composed
// separately because they do not use the same class-layer parent transform.
inline game_scene_render_queue::Queue apply_static_world_camera(
        game_scene_render_queue::Queue queue,
        float camera_eye_x) {
    for (auto& sprite : queue.sprites) {
        const auto translation = game_scene_camera_contract::static_screen_translation_x(
                sprite.source_layer_index,
                sprite.layer_camera_factor,
                camera_eye_x);
        if (!translation.has_value()) continue;
        sprite.transform.position_x += *translation;
    }
    return queue;
}

}  // namespace nevergone::game_scene_camera_render_queue
