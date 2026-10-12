#include "controls_function_state_runtime.h"

#include <algorithm>
#include <mutex>
#include <sstream>

#include "controls_function_state_contract.h"
#include "game_clock.h"
#include "game_scene_action_surface_input.h"
#include "game_scene_player_joystick_input.h"
#include "game_scene_player_spawn_state.h"
#include "player_function_action_contract.h"

namespace nevergone::controls_function_state_runtime {
namespace {

std::mutex g_mutex;
Snapshot g_snapshot{};
std::uint64_t g_generation = 0;
std::uint32_t g_consumed_combo_state = 0;
std::uint64_t g_consumed_combo_started_tick = 0;

bool primary_combo(std::uint32_t state) {
    return state == 1u || state == 2u || state == 10u || state == 11u;
}

bool secondary_combo(std::uint32_t state) {
    return state >= 14u && state <= 19u;
}

float elapsed_seconds(std::uint64_t game_tick, std::uint64_t started_tick) {
    if (game_tick <= started_tick) return 0.0f;
    return static_cast<float>(
        static_cast<double>(game_tick - started_tick) * game_clock::kFixedStepSeconds);
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_generation;
    g_snapshot = {};
    g_snapshot.generation = g_generation;
    g_consumed_combo_state = 0;
    g_consumed_combo_started_tick = 0;
}

bool advance_sample(
        bool player_available,
        const std::array<bool, 3>& pressed,
        std::uint32_t source_direction_combo_state,
        std::uint64_t source_combo_started_tick,
        std::uint64_t game_tick) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (source_direction_combo_state == 0u ||
        source_direction_combo_state != g_consumed_combo_state ||
        source_combo_started_tick != g_consumed_combo_started_tick) {
        if (source_direction_combo_state == 0u ||
            source_direction_combo_state != g_consumed_combo_state ||
            source_combo_started_tick != g_consumed_combo_started_tick) {
            // A changed source identity is a new Controls +0x110 event.
            if (source_direction_combo_state != g_consumed_combo_state ||
                source_combo_started_tick != g_consumed_combo_started_tick) {
                g_consumed_combo_state = 0;
                g_consumed_combo_started_tick = 0;
            }
        }
    }

    const bool consumed =
        source_direction_combo_state != 0u &&
        source_direction_combo_state == g_consumed_combo_state &&
        source_combo_started_tick == g_consumed_combo_started_tick;
    const std::uint32_t effective_combo = consumed ? 0u : source_direction_combo_state;

    const float elapsed = elapsed_seconds(game_tick, source_combo_started_tick);
    const float primary_timer = primary_combo(effective_combo) ? elapsed : 0.0f;
    const float secondary_timer = secondary_combo(effective_combo) ? elapsed : 0.0f;

    controls_function_state_contract::Input input{};
    input.player_available = player_available;
    input.button_a_pressed = pressed[0];
    input.button_b_pressed = pressed[1];
    input.button_c_pressed = pressed[2];
    input.previous_function_state = g_snapshot.function_state;
    input.direction_combo_state = effective_combo;
    input.primary_combo_timer_seconds = primary_timer;
    input.secondary_combo_timer_seconds = secondary_timer;
    // Timers are reconstructed from the source event's logical start tick, so
    // the pure step receives an absolute timer sample rather than incrementing it.
    input.delta_seconds = 0.0f;

    const auto output = controls_function_state_contract::step(input);
    if (!output.has_value()) return false;

    if (effective_combo != 0u && output->direction_combo_state == 0u) {
        g_consumed_combo_state = source_direction_combo_state;
        g_consumed_combo_started_tick = source_combo_started_tick;
    }

    ++g_generation;
    Snapshot next{};
    next.generation = g_generation;
    next.game_tick = game_tick;
    next.player_available = player_available;
    next.pressed = pressed;
    next.function_state = output->function_state;
    next.source_direction_combo_state = source_direction_combo_state;
    next.effective_direction_combo_state = effective_combo;
    next.synthesized_direction_combo_state = output->direction_combo_state;
    next.primary_combo_timer_seconds = output->primary_combo_timer_seconds;
    next.secondary_combo_timer_seconds = output->secondary_combo_timer_seconds;
    next.source_combo_consumed =
        source_direction_combo_state != 0u &&
        source_direction_combo_state == g_consumed_combo_state &&
        source_combo_started_tick == g_consumed_combo_started_tick;
    next.consumed_combo_started_tick = g_consumed_combo_started_tick;

    const auto request = player_function_action_contract::base_action_request(next.function_state);
    if (request.has_value()) {
        next.has_pending_base_action = true;
        next.pending_meplayer_state = request->meplayer_state;
        next.pending_force = request->force;
        next.pending_has_additional_guard = request->has_additional_player_guard;
    }

    g_snapshot = next;
    return true;
}

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

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Controls function-state runtime\n";
    out << "generation: " << state.generation << "\n";
    out << "game tick: " << state.game_tick << "\n";
    out << "player available: " << (state.player_available ? "yes" : "no") << "\n";
    out << "pressed A/B/C: "
        << (state.pressed[0] ? 1 : 0) << "/"
        << (state.pressed[1] ? 1 : 0) << "/"
        << (state.pressed[2] ? 1 : 0) << "\n";
    out << "function state: 0x" << std::hex << state.function_state << std::dec << "\n";
    out << "source combo: 0x" << std::hex << state.source_direction_combo_state
        << " effective=0x" << state.effective_direction_combo_state
        << " synthesized=0x" << state.synthesized_direction_combo_state << std::dec << "\n";
    out << "combo consumed: " << (state.source_combo_consumed ? "yes" : "no") << "\n";
    if (state.has_pending_base_action) {
        out << "pending MEPlayer state: 0x" << std::hex << state.pending_meplayer_state
            << std::dec << " force=" << (state.pending_force ? "yes" : "no")
            << " extra-guard=" << (state.pending_has_additional_guard ? "yes" : "no") << "\n";
    } else {
        out << "pending MEPlayer state: none\n";
    }
    return out.str();
}

}  // namespace nevergone::controls_function_state_runtime
