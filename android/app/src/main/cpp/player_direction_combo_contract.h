#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace nevergone::player_direction_combo_contract {

// Horizontal Controls::comboJoyStickUpdate() subset recovered from the shipped
// ARMv7 implementation. These are the exact retained Controls +0x110 values
// that feed MEPlayer::setDirComboState().
constexpr std::uint32_t kFastLeftState = 0x0fu;
constexpr std::uint32_t kFastRightState = 0x11u;
constexpr std::uint32_t kLeftState = 0x12u;
constexpr std::uint32_t kRightState = 0x13u;

constexpr float kFastWindowMinSeconds = 0.03f;
constexpr float kFastWindowMaxSeconds = 0.40f;
constexpr float kDeactivateExpirySeconds = 0.50f;

enum class HorizontalSide {
    kNone = 0,
    kLeft,
    kRight,
};

inline HorizontalSide horizontal_side(float angle_degrees) {
    if (!std::isfinite(angle_degrees) ||
        angle_degrees < -180.0f || angle_degrees > 180.0f) {
        return HorizontalSide::kNone;
    }
    // Controls::comboJoyStickUpdate(): right is the strict (-45, 45)
    // sector; left is the strict outside-of-[-135, 135] sector.
    if (angle_degrees > -45.0f && angle_degrees < 45.0f) {
        return HorizontalSide::kRight;
    }
    if (angle_degrees < -135.0f || angle_degrees > 135.0f) {
        return HorizontalSide::kLeft;
    }
    return HorizontalSide::kNone;
}

inline std::optional<std::uint32_t> normal_state(HorizontalSide side) {
    switch (side) {
        case HorizontalSide::kLeft: return kLeftState;
        case HorizontalSide::kRight: return kRightState;
        case HorizontalSide::kNone: break;
    }
    return std::nullopt;
}

inline std::optional<std::uint32_t> fast_state(HorizontalSide side) {
    switch (side) {
        case HorizontalSide::kLeft: return kFastLeftState;
        case HorizontalSide::kRight: return kFastRightState;
        case HorizontalSide::kNone: break;
    }
    return std::nullopt;
}

inline bool same_normal_side(std::uint32_t retained_state, HorizontalSide side) {
    const auto expected = normal_state(side);
    return expected.has_value() && retained_state == *expected;
}

inline bool in_fast_reactivation_window(float elapsed_seconds) {
    return std::isfinite(elapsed_seconds) &&
           elapsed_seconds > kFastWindowMinSeconds &&
           elapsed_seconds < kFastWindowMaxSeconds;
}

inline bool expires_on_deactivate(float elapsed_seconds) {
    return std::isfinite(elapsed_seconds) &&
           elapsed_seconds > kDeactivateExpirySeconds;
}

inline std::optional<std::uint32_t> fast_reactivation_state(
        std::uint32_t retained_state,
        HorizontalSide side,
        float elapsed_seconds) {
    if (!same_normal_side(retained_state, side) ||
        !in_fast_reactivation_window(elapsed_seconds)) {
        return std::nullopt;
    }
    return fast_state(side);
}

inline bool is_fast_state(std::uint32_t state) {
    return state == kFastLeftState || state == kFastRightState;
}

}  // namespace nevergone::player_direction_combo_contract
