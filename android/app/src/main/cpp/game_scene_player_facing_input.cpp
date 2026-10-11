#include "game_scene_player_facing_input.h"

#include <cmath>
#include <mutex>

#include "player_control_runtime_state.h"

namespace nevergone::game_scene_player_facing_input {
namespace {
std::mutex g_mutex;
Gesture g_gesture;
}

void Gesture::reset() {
    *this = Gesture{};
}

bool Gesture::on_touch(int action, int pointer, float x, float y,
                       int width, int height, std::size_t index,
                       const std::string& guid) {
    if (width <= 0 || height <= 0 || guid.empty() ||
        !std::isfinite(x) || !std::isfinite(y)) return false;
    if (scene_index != index || scene_guid != guid) {
        reset();
        scene_index = index;
        scene_guid = guid;
        player_control_runtime_state::reset();
    }
    // ACTION_DOWN / ACTION_POINTER_DOWN. A lower-left drag is the minimal
    // offline control surface until the recovered joystick artwork is staged.
    if (action == 0 || action == 5) {
        if (tracking || x < 0.0f || x > width * 0.4f ||
            y < height * 0.55f || y > height) return false;
        tracking = true;
        pointer_id = pointer;
        down_x = x;
        return true;
    }
    if (!tracking || pointer != pointer_id) return false;
    if (action == 2) {
        const float activity = std::fabs(x - down_x) / static_cast<float>(width);
        const float angle = x >= down_x ? 0.0f : 180.0f;
        (void)player_control_runtime_state::apply_horizontal_sample(activity, angle);
        return true;
    }
    if (action == 1 || action == 3 || action == 6) {
        tracking = false;
        pointer_id = -1;
        return true;
    }
    return false;
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_gesture.reset();
    player_control_runtime_state::reset();
}

bool on_touch(int action, int pointer, float x, float y, int width, int height,
              std::size_t index, const std::string& guid) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_gesture.on_touch(action, pointer, x, y, width, height, index, guid);
}

}  // namespace nevergone::game_scene_player_facing_input
