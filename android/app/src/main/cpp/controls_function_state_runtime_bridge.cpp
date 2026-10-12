#include "controls_function_state_runtime.h"

#include "game_clock.h"
#include "game_scene_action_surface_input.h"
#include "game_scene_player_joystick_input.h"
#include "game_scene_player_spawn_state.h"

namespace nevergone::controls_function_state_runtime {

bool tick() {
    const auto action = game_scene_action_surface_input::snapshot();
    const auto joystick = game_scene_player_joystick_input::snapshot();
    const auto spawn = game_scene_player_spawn_state::snapshot();
    const bool player_available =
        spawn.status == game_scene_player_spawn_state::Status::kReady;

    return advance_sample(
        player_available,
        action.pressed,
        joystick.direction_combo_state,
        joystick.direction_combo_started_tick,
        game_clock::tick_count());
}

}  // namespace nevergone::controls_function_state_runtime
