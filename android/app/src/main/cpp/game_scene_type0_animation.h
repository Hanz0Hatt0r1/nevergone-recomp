#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "game_scene_type0_post_init.h"

namespace nevergone::game_scene_type0_animation {

struct Sample {
    float skew_parameter = 0.0f;
    std::uint8_t opacity = 255u;
};

inline float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

inline Sample evaluate_random_skew_loop(
        const game_scene_type0_post_init::RandomSkewLoopParams& params,
        double elapsed_seconds) {
    Sample sample;
    if (!(params.segment_duration_seconds > 0.0f) || !std::isfinite(elapsed_seconds) ||
        elapsed_seconds <= 0.0) {
        return sample;
    }

    const double duration = static_cast<double>(params.segment_duration_seconds);
    const float target = params.skew_parameter;

    // The first CCSkewTo begins at the sprite's zero post-init skew. Once the
    // first pair completes, CCRepeatForever continuously alternates -target and
    // +target with the same segment duration.
    if (elapsed_seconds < duration) {
        const float t = clamp01(static_cast<float>(elapsed_seconds / duration));
        sample.skew_parameter = target * t;
        return sample;
    }

    const double after_first = elapsed_seconds - duration;
    const double segment_index_real = std::floor(after_first / duration);
    const std::uint64_t segment_index = static_cast<std::uint64_t>(segment_index_real);
    const double segment_start = segment_index_real * duration;
    const float t = clamp01(static_cast<float>((after_first - segment_start) / duration));

    if ((segment_index & 1u) == 0u) {
        sample.skew_parameter = target + (-2.0f * target * t);
    } else {
        sample.skew_parameter = -target + (2.0f * target * t);
    }
    return sample;
}

inline Sample evaluate_random_fade_loop(
        const game_scene_type0_post_init::RandomFadeLoopParams& params,
        double elapsed_seconds) {
    Sample sample;
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds <= 0.0 ||
        !(params.delay_seconds >= 0.0f) || !(params.fade_duration_seconds > 0.0f)) {
        return sample;
    }

    const double delay = static_cast<double>(params.delay_seconds);
    const double fade = static_cast<double>(params.fade_duration_seconds);
    const double period = delay + fade + fade;
    if (!(period > 0.0)) return sample;

    double phase = std::fmod(elapsed_seconds, period);
    if (phase < 0.0) phase += period;
    if (phase < delay) {
        sample.opacity = params.high_opacity;
        return sample;
    }

    phase -= delay;
    if (phase < fade) {
        const float t = clamp01(static_cast<float>(phase / fade));
        const float value = static_cast<float>(params.high_opacity) +
            (static_cast<float>(params.low_opacity) - static_cast<float>(params.high_opacity)) * t;
        sample.opacity = static_cast<std::uint8_t>(static_cast<int>(value));
        return sample;
    }

    phase -= fade;
    const float t = clamp01(static_cast<float>(phase / fade));
    const float value = static_cast<float>(params.low_opacity) +
        (static_cast<float>(params.high_opacity) - static_cast<float>(params.low_opacity)) * t;
    sample.opacity = static_cast<std::uint8_t>(static_cast<int>(value));
    return sample;
}

}  // namespace nevergone::game_scene_type0_animation
