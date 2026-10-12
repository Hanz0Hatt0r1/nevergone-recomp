#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace nevergone::controls_function_state_runtime {

struct Snapshot {
    std::uint64_t generation = 0;
    std::uint64_t game_tick = 0;
    bool player_available = false;
    std::array<bool, 3> pressed{};
    std::uint32_t function_state = 0;
    std::uint32_t source_direction_combo_state = 0;
    std::uint32_t effective_direction_combo_state = 0;
    std::uint32_t synthesized_direction_combo_state = 0;
    float primary_combo_timer_seconds = 0.0f;
    float secondary_combo_timer_seconds = 0.0f;
    bool source_combo_consumed = false;
    std::uint64_t consumed_combo_started_tick = 0;
    std::uint32_t pending_meplayer_state = 0;
    bool pending_force = false;
    bool pending_has_additional_guard = false;
    bool has_pending_base_action = false;
};

void reset();

// Stateful test/runtime boundary. source_combo_started_tick identifies one
// retained Controls +0x110 event; once the recovered Controls::tick transition
// consumes that event, repeated samples with the same identity stay neutral.
bool advance_sample(
    bool player_available,
    const std::array<bool, 3>& pressed,
    std::uint32_t source_direction_combo_state,
    std::uint64_t source_combo_started_tick,
    std::uint64_t game_tick);

// Collects the live action-button, joystick and player-spawn samples.
bool tick();
Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::controls_function_state_runtime
