#include "player_control_contract.h"

#include <cmath>

namespace nevergone::player_control_contract {

std::optional<PrimaryControlState> horizontal_primary_state(
        float activity_metric,
        float angle_degrees) {
    if (!std::isfinite(activity_metric) || !std::isfinite(angle_degrees)) {
        return std::nullopt;
    }
    if (activity_metric <= kHorizontalActivityThreshold) return std::nullopt;

    if (angle_degrees >= kNegativeBoundaryDegrees &&
            angle_degrees <= kPositiveBoundaryDegrees) {
        return PrimaryControlState::kState13;
    }
    if (angle_degrees >= kPositiveBoundaryDegrees) {
        return PrimaryControlState::kState12;
    }
    if (angle_degrees >= kNegativeWrapDegrees) {
        return PrimaryControlState::kState12;
    }
    return std::nullopt;
}

std::optional<bool> facing_flip_x(PrimaryControlState state) {
    switch (state) {
        case PrimaryControlState::kState12: return true;
        case PrimaryControlState::kState13: return false;
    }
    return std::nullopt;
}

std::array<TickDispatch, 4> tick_dispatches(
        std::int32_t dir_combo,
        std::int32_t direction,
        std::int32_t function,
        std::int32_t primary) {
    return {{
        {TickTarget::kDirCombo, dir_combo},
        {TickTarget::kDirection, direction},
        {TickTarget::kFunction, function},
        {TickTarget::kPrimary, primary},
    }};
}

const char* state_name(PrimaryControlState state) {
    switch (state) {
        case PrimaryControlState::kState12: return "state-0x12";
        case PrimaryControlState::kState13: return "state-0x13";
    }
    return "unknown";
}

const char* target_name(TickTarget target) {
    switch (target) {
        case TickTarget::kDirCombo: return "dir-combo";
        case TickTarget::kDirection: return "direction";
        case TickTarget::kFunction: return "function";
        case TickTarget::kPrimary: return "primary";
    }
    return "unknown";
}

}  // namespace nevergone::player_control_contract
