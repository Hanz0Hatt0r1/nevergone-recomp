#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "single_select_hero_coffin_layout.h"

namespace nevergone::single_select_hero_coffin_motion {

// ARM32 SingleSelectHero::Carousel 0x004389ce..0x00438ac8 and
// SingleSelectHero::OpenTheDoor 0x00437f00..0x00438390.
// This is an evidence-linked action model, not yet an approved lid renderer.
// Source: the original libcocos2dcpp.so SHA-256 94b1ef6a...32920828eb5e.
constexpr float kNearReelMoveSeconds = 1.5f;
constexpr float kFarReelMoveSeconds = 3.0f;
constexpr float kReelBounceSeconds = 0.3f;
constexpr float kReelSettleSeconds = 0.6f;
constexpr float kReelBounceUnits = 3.0f;

constexpr float kLidCloseMoveSeconds = 2.0f;
constexpr float kLidCloseBounceSeconds = 0.3f;
constexpr float kLidCloseSettleSeconds = 0.39f;
constexpr float kLidCloseBounceUnits = 3.0f;
constexpr float kLidOpenNudgeSeconds = 0.2f;
constexpr float kLidOpenHoldSeconds = 0.5f;
constexpr float kLidOpenMoveSeconds = 2.0f;
constexpr float kLidOpenBounceSeconds = 0.3f;
constexpr float kLidOpenSettleSeconds = 0.39f;
constexpr float kLidOpenNudgeUnits = 8.0f;
constexpr float kLidOpenBounceUnits = 3.0f;

constexpr float kCloseTotalSeconds =
    kLidCloseMoveSeconds + kLidCloseBounceSeconds + kLidCloseSettleSeconds;
constexpr float kOpenTotalSeconds =
    kLidOpenNudgeSeconds + kLidOpenHoldSeconds + kLidOpenMoveSeconds +
    kLidOpenBounceSeconds + kLidOpenSettleSeconds;

constexpr float kPi = 3.14159265358979323846f;
inline float clamp01(float x) {
    if (!std::isfinite(x)) return 0.0f;
    return std::max(0.0f, std::min(x, 1.0f));
}
inline float sine_in(float x) {
    return 1.0f - std::cos(clamp01(x) * kPi * 0.5f);
}
inline float sine_out(float x) {
    return std::sin(clamp01(x) * kPi * 0.5f);
}
inline float ease_in_out(float x, float rate) {
    const float t = clamp01(x);
    return t < 0.5f
        ? 0.5f * std::pow(2.0f * t, rate)
        : 1.0f - 0.5f * std::pow(2.0f * (1.0f - t), rate);
}
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}
inline float seconds_from_ticks(std::uint64_t now, std::uint64_t start,
                                float tick_rate = 35.0f) {
    return now >= start && tick_rate > 0.0f && std::isfinite(tick_rate)
        ? static_cast<float>(now - start) / tick_rate : 0.0f;
}
inline bool supported_career(std::int64_t career) {
    return career >= 1 && career <= single_select_hero_coffin_layout::kLidCopies;
}
inline float reel_first_seconds(std::int64_t old_career, std::int64_t new_career) {
    if (!supported_career(old_career) || !supported_career(new_career)) return 0.0f;
    return std::abs(new_career - old_career) <= 2
        ? kNearReelMoveSeconds : kFarReelMoveSeconds;
}
inline float reel_total_seconds(std::int64_t old_career, std::int64_t new_career) {
    const float first = reel_first_seconds(old_career, new_career);
    return first <= 0.0f ? 0.0f : first + kReelBounceSeconds + kReelSettleSeconds;
}

// Three original reel CCMoveTo actions, with CCEaseInOut(rate=first_duration),
// CCEaseSineOut and CCEaseSineIn respectively. Caller supplies elapsed
// seconds since Carousel() began. Same-career initial Carousel bounces -3.
inline float reel_x(std::int64_t old_career, std::int64_t new_career,
                    float seconds) {
    if (!supported_career(old_career) || !supported_career(new_career)) return 0.0f;
    const float start =
        single_select_hero_coffin_layout::settled_reel_offset_x(old_career);
    const float end =
        single_select_hero_coffin_layout::settled_reel_offset_x(new_career);
    const float first = reel_first_seconds(old_career, new_career);
    const float bounce = end +
        (new_career > old_career ? kReelBounceUnits : -kReelBounceUnits);
    if (seconds <= 0.0f) return start;
    if (seconds < first)
        return lerp(start, end, ease_in_out(seconds / first, first));
    seconds -= first;
    if (seconds < kReelBounceSeconds)
        return lerp(end, bounce, sine_out(seconds / kReelBounceSeconds));
    seconds -= kReelBounceSeconds;
    if (seconds < kReelSettleSeconds)
        return lerp(bounce, end, sine_in(seconds / kReelSettleSeconds));
    return end;
}

// OpenTheDoor(false) closes the previously opened lid. The 0->controlPoint
// start is conditional on the lid having completed its earlier open action.
inline float closing_lid_x(float closed_x, float seconds) {
    if (seconds <= 0.0f) return 0.0f;
    if (seconds < kLidCloseMoveSeconds)
        return lerp(0.0f, closed_x, sine_in(seconds / kLidCloseMoveSeconds));
    seconds -= kLidCloseMoveSeconds;
    if (seconds < kLidCloseBounceSeconds)
        return lerp(closed_x, closed_x - kLidCloseBounceUnits,
                    sine_out(seconds / kLidCloseBounceSeconds));
    seconds -= kLidCloseBounceSeconds;
    if (seconds < kLidCloseSettleSeconds)
        return lerp(closed_x - kLidCloseBounceUnits, closed_x,
                    sine_in(seconds / kLidCloseSettleSeconds));
    return closed_x;
}

// OpenTheDoor(true): move 0.2 s to controlPointX-8; delay 0.5 s;
// move 2.0 s to controlPointX-initialPositionX; overshoot +3 for 0.3 s;
// return in 0.39 s. We retain the original initialPositionX dependency:
// 0 is the final target ONLY if the lid started closed at controlPointX.
inline float opening_lid_x(float closed_x, float initial_x, float seconds) {
    const float target = closed_x - initial_x;
    if (seconds <= 0.0f) return initial_x;
    if (seconds < kLidOpenNudgeSeconds)
        return lerp(initial_x, closed_x - kLidOpenNudgeUnits,
                    sine_in(seconds / kLidOpenNudgeSeconds));
    seconds -= kLidOpenNudgeSeconds;
    if (seconds < kLidOpenHoldSeconds) return closed_x - kLidOpenNudgeUnits;
    seconds -= kLidOpenHoldSeconds;
    if (seconds < kLidOpenMoveSeconds)
        return lerp(closed_x - kLidOpenNudgeUnits, target,
                    sine_in(seconds / kLidOpenMoveSeconds));
    seconds -= kLidOpenMoveSeconds;
    if (seconds < kLidOpenBounceSeconds)
        return lerp(target, target + kLidOpenBounceUnits,
                    sine_out(seconds / kLidOpenBounceSeconds));
    seconds -= kLidOpenBounceSeconds;
    if (seconds < kLidOpenSettleSeconds)
        return lerp(target + kLidOpenBounceUnits, target,
                    sine_in(seconds / kLidOpenSettleSeconds));
    return target;
}

}  // namespace nevergone::single_select_hero_coffin_motion
