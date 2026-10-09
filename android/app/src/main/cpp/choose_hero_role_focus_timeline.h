#pragma once

#include <cstddef>

namespace nevergone::choose_hero_role_focus_timeline {

constexpr float kInitialDelaySeconds = 1.0f;
constexpr float kSegmentSeconds = 1.25f;
constexpr float kResetSeconds = 0.01f;
constexpr float kTailDelaySeconds = 2.0f;
constexpr float kPeakAlpha = 153.0f / 255.0f;
constexpr float kTopInsetX = 100.0f;
constexpr float kTopYInset = 11.5f;
constexpr float kBottomY = 9.5f;
constexpr float kCycleSeconds =
    kInitialDelaySeconds + kSegmentSeconds + kSegmentSeconds +
    kResetSeconds + kTailDelaySeconds;

struct HighlightPose {
    float x = 0.0f;
    float y = 0.0f;
    float alpha = 0.0f;
};

// Reconstructs the two act_hilight.png CCRepeatForever actions created by
// ChooseHeroItem::focesItem(). index 0 is the top left-to-right highlight;
// index 1 is the bottom right-to-left highlight.
bool sample(
    std::size_t index,
    double elapsed_seconds,
    float board_width,
    float board_height,
    float highlight_width,
    HighlightPose* output);

}  // namespace nevergone::choose_hero_role_focus_timeline
