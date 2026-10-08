#include "splash_timeline.h"

#include <algorithm>

#include "game_clock.h"

namespace nevergone::splash_timeline {
namespace {

float clamp01(double value) {
    if (value <= 0.0) return 0.0f;
    if (value >= 1.0) return 1.0f;
    return static_cast<float>(value);
}

float fade_in(double seconds, double start, double duration) {
    return clamp01((seconds - start) / duration);
}

float fade_out(double seconds, double start, double duration) {
    return 1.0f - clamp01((seconds - start) / duration);
}

float windowed_fade(
    double seconds,
    double fade_in_start,
    double fade_in_duration,
    double fade_out_start,
    double fade_out_duration) {
    if (seconds < fade_in_start) return 0.0f;
    if (seconds < fade_in_start + fade_in_duration) {
        return fade_in(seconds, fade_in_start, fade_in_duration);
    }
    if (seconds < fade_out_start) return 1.0f;
    if (seconds < fade_out_start + fade_out_duration) {
        return fade_out(seconds, fade_out_start, fade_out_duration);
    }
    return 0.0f;
}

}  // namespace

Sample sample(double seconds) {
    seconds = std::max(0.0, seconds);

    Sample result;

    // HelloWorld::FuncNEND6 creates an opaque black CCLayerColor and immediately
    // runs CCFadeOut(3.0f) on it.
    result.black_overlay_alpha = fade_out(seconds, 0.0, 3.0);

    // HIPPIEGOLO01/02 are visible from creation and both reach their final
    // 0.5-second fade-out at t=5.0. HIPPIEGOLO02 then triggers FuncNEND2,
    // which removes the splash node and proceeds to createUI().
    const float base_alpha = seconds < 5.0
        ? 1.0f
        : (seconds < 5.5 ? fade_out(seconds, 5.0, 0.5) : 0.0f);
    result.frame_alpha[0] = base_alpha;
    result.frame_alpha[1] = base_alpha;

    // HIPPIEGOLO03: Delay(1.0), Delay(1.0), FadeIn(0.5), Delay(0.5),
    // FadeOut(1.0), FuncNEND.
    result.frame_alpha[2] = windowed_fade(seconds, 2.0, 0.5, 3.0, 1.0);

    // HIPPIEGOLO04: Delay(1.2), FadeIn(0.5), FadeOut(0.7), FuncNEND.
    result.frame_alpha[3] = windowed_fade(seconds, 1.2, 0.5, 1.7, 0.7);

    // HIPPIEGOLO05: Delay(1.5), FadeIn(0.5), FadeOut(0.7), FuncNEND.
    result.frame_alpha[4] = windowed_fade(seconds, 1.5, 0.5, 2.0, 0.7);

    // HIPPIEGOLO06: Delay(1.8), FadeIn(0.5), FadeOut(0.7), FuncNEND.
    result.frame_alpha[5] = windowed_fade(seconds, 1.8, 0.5, 2.3, 0.7);

    result.complete = seconds >= kTimelineCompleteSeconds;
    return result;
}

Sample sample_tick(std::uint64_t tick) {
    return sample(static_cast<double>(tick) * nevergone::game_clock::kFixedStepSeconds);
}

}  // namespace nevergone::splash_timeline
