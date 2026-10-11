#pragma once

#include <cstddef>
#include <string>

namespace nevergone::game_scene_player_facing_input {

// A small project-owned horizontal gesture. The shipped control states and
// their facing effect are recovered; the touch area is a temporary offline UI.
struct Gesture {
    std::size_t scene_index = 0;
    std::string scene_guid;
    int pointer_id = -1;
    float down_x = 0.0f;
    bool tracking = false;

    void reset();
    bool on_touch(int action, int pointer, float x, float y,
                  int width, int height, std::size_t index,
                  const std::string& guid);
};

void reset();
bool on_touch(int action, int pointer, float x, float y, int width, int height,
              std::size_t index, const std::string& guid);

}  // namespace nevergone::game_scene_player_facing_input
