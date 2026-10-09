#include "choose_hero_role_focus_timeline.h"

#include <algorithm>
#include <cmath>

namespace nevergone::choose_hero_role_focus_timeline {
namespace {

float lerp(float a, float b, double t) {
    return a + (b - a) * static_cast<float>(t);
}

float segment_alpha(double phase) {
    const double first_begin = kInitialDelaySeconds;
    const double first_end = first_begin + kSegmentSeconds;
    const double second_end = first_end + kSegmentSeconds;
    if (phase < first_begin || phase >= second_end) return 0.0f;
    if (phase < first_end) {
        const double t = (phase - first_begin) / kSegmentSeconds;
        return kPeakAlpha * static_cast<float>(std::clamp(t, 0.0, 1.0));
    }
    const double t = (phase - first_end) / kSegmentSeconds;
    return kPeakAlpha * (1.0f - static_cast<float>(std::clamp(t, 0.0, 1.0)));
}

}  // namespace

bool sample(
        std::size_t index,
        double elapsed_seconds,
        float board_width,
        float board_height,
        float highlight_width,
        HighlightPose* output) {
    if (output == nullptr || index > 1 || !std::isfinite(elapsed_seconds) ||
            !std::isfinite(board_width) || !std::isfinite(board_height) ||
            !std::isfinite(highlight_width) || board_width <= 0.0f ||
            board_height <= 0.0f || highlight_width <= 0.0f) {
        return false;
    }

    const double clamped = std::max(0.0, elapsed_seconds);
    double phase = std::fmod(clamped, static_cast<double>(kCycleSeconds));
    if (phase < 0.0) phase += kCycleSeconds;

    const float left = kTopInsetX + highlight_width * 0.5f;
    const float right = board_width - highlight_width * 0.5f;
    const float midpoint = kTopInsetX + (right - kTopInsetX) * 0.5f;

    float x = index == 0 ? left : right;
    const double first_begin = kInitialDelaySeconds;
    const double first_end = first_begin + kSegmentSeconds;
    const double second_end = first_end + kSegmentSeconds;
    const double reset_end = second_end + kResetSeconds;

    if (phase >= first_begin && phase < first_end) {
        const double t = (phase - first_begin) / kSegmentSeconds;
        x = index == 0 ? lerp(left, midpoint, t) : lerp(right, midpoint, t);
    } else if (phase >= first_end && phase < second_end) {
        const double t = (phase - first_end) / kSegmentSeconds;
        x = index == 0 ? lerp(midpoint, right, t) : lerp(midpoint, left, t);
    } else if (phase >= second_end && phase < reset_end) {
        const double t = (phase - second_end) / kResetSeconds;
        x = index == 0 ? lerp(right, left, t) : lerp(left, right, t);
    }

    output->x = x;
    output->y = index == 0 ? board_height - kTopYInset : kBottomY;
    output->alpha = segment_alpha(phase);
    return true;
}

}  // namespace nevergone::choose_hero_role_focus_timeline
