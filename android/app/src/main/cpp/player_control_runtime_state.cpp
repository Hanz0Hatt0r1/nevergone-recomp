#include "player_control_runtime_state.h"

#include <mutex>
#include <sstream>

namespace nevergone::player_control_runtime_state {
namespace {

std::mutex g_mutex;
std::uint64_t g_generation = 0;
Snapshot g_snapshot;

const char* primary_state_report_name(
        const std::optional<player_control_contract::PrimaryControlState>& state) {
    if (!state.has_value()) return "none";
    switch (*state) {
        case player_control_contract::PrimaryControlState::kState12: return "0x12";
        case player_control_contract::PrimaryControlState::kState13: return "0x13";
    }
    return "invalid";
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_generation;
    g_snapshot = {};
    g_snapshot.generation = g_generation;
}

bool apply_primary_state(player_control_contract::PrimaryControlState state) {
    const auto flip_x = player_control_contract::facing_flip_x(state);
    if (!flip_x.has_value()) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_snapshot.primary_state == state && g_snapshot.flip_x == *flip_x) {
        return true;
    }

    ++g_generation;
    g_snapshot.generation = g_generation;
    g_snapshot.primary_state = state;
    g_snapshot.flip_x = *flip_x;
    return true;
}

bool apply_horizontal_sample(float activity_metric, float angle_degrees) {
    const auto state = player_control_contract::horizontal_primary_state(
            activity_metric, angle_degrees);
    return state.has_value() && apply_primary_state(*state);
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Player control runtime\n";
    out << "generation: " << state.generation << "\n";
    out << "primary state: " << primary_state_report_name(state.primary_state) << "\n";
    out << "flip x: " << (state.flip_x ? "true" : "false") << "\n";
    return out.str();
}

}  // namespace nevergone::player_control_runtime_state
