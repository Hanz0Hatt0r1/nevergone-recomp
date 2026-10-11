#pragma once

#include <utility>

#include "game_scene_player_joystick_render_queue.h"
#include "game_scene_player_render_queue.h"
#include "game_scene_render_queue.h"

namespace nevergone::game_scene_runtime_render_queue {

// Keep JNI asset-request staging and GL drawing on one canonical composition
// path. The recovered player body is inserted at its world/UI boundary first;
// the fixed Controls joystick dock is then appended as the screen-space overlay.
inline game_scene_render_queue::Queue current(game_scene_render_queue::Queue queue) {
    queue = game_scene_player_render_queue::current(std::move(queue));
    return game_scene_player_joystick_render_queue::current(std::move(queue));
}

}  // namespace nevergone::game_scene_runtime_render_queue
