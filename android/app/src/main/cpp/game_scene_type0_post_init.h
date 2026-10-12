#pragma once

#include <cstdint>
#include <string>

namespace nevergone::game_scene_type0_post_init {

enum class Behavior {
    kNone = 0,
    // Recovered GameSceneObject::initWithData() gkshu01..08 branch.
    // The original applies randomized skew and runs the resulting sequence forever.
    kRandomSkewLoop,
    // Recovered GameSceneObject::initWithData() gkdeng01..07 branch.
    // The original applies randomized delay/fade timing and repeats it forever.
    kRandomFadeLoop,
};

struct RandomSkewLoopParams {
    float segment_duration_seconds = 1.0f;
    // Passed to CCSkewTo as the randomized non-zero skew parameter. The second
    // action uses its negation; the other skew parameter remains zero.
    float skew_parameter = 2.0f;
};

struct RandomFadeLoopParams {
    float delay_seconds = 6.0f;
    float fade_duration_seconds = 2.0f;
    std::uint8_t low_opacity = 0u;
    std::uint8_t high_opacity = 255u;
};

// ARMv7 lrand48() returns a non-negative 31-bit sample. The recovered code
// multiplies its signed-float conversion by 2^-31 (0x30000000 as float bits in
// the literal pool), producing [0, 1).
inline float normalize_lrand48(std::uint32_t sample) {
    constexpr float kLrand48Unit = 1.0f / 2147483648.0f;
    return static_cast<float>(sample & 0x7fffffffu) * kLrand48Unit;
}

inline RandomSkewLoopParams make_random_skew_loop_params(
        std::uint32_t duration_sample,
        std::uint32_t skew_sample) {
    RandomSkewLoopParams params;
    params.segment_duration_seconds = 1.0f + 1.5f * normalize_lrand48(duration_sample);
    params.skew_parameter = 2.0f + 1.5f * normalize_lrand48(skew_sample);
    return params;
}

inline RandomFadeLoopParams make_random_fade_loop_params(
        std::uint32_t opacity_sample,
        std::uint32_t delay_sample) {
    RandomFadeLoopParams params;
    params.delay_seconds = 6.0f + 3.0f * normalize_lrand48(delay_sample);
    // Recovered VFP path: normalized * 10 * 255 / 100, convert to signed int,
    // then UXTB before CCFadeTo. Inputs are non-negative, so C++ truncation
    // matches the effective integer result here.
    const float low = normalize_lrand48(opacity_sample) * 10.0f * 255.0f / 100.0f;
    params.low_opacity = static_cast<std::uint8_t>(static_cast<int>(low));
    return params;
}

inline Behavior classify(const std::string& serialized_name) {
    if (serialized_name == "gkshu01.png" ||
        serialized_name == "gkshu02.png" ||
        serialized_name == "gkshu03.png" ||
        serialized_name == "gkshu04.png" ||
        serialized_name == "gkshu05.png" ||
        serialized_name == "gkshu06.png" ||
        serialized_name == "gkshu07.png" ||
        serialized_name == "gkshu08.png") {
        return Behavior::kRandomSkewLoop;
    }

    if (serialized_name == "gkdeng01.png" ||
        serialized_name == "gkdeng02.png" ||
        serialized_name == "gkdeng03.png" ||
        serialized_name == "gkdeng04.png" ||
        serialized_name == "gkdeng05.png" ||
        serialized_name == "gkdeng06.png" ||
        serialized_name == "gkdeng07.png") {
        return Behavior::kRandomFadeLoop;
    }

    return Behavior::kNone;
}

inline const char* behavior_name(Behavior behavior) {
    switch (behavior) {
        case Behavior::kNone: return "none";
        case Behavior::kRandomSkewLoop: return "random-skew-loop";
        case Behavior::kRandomFadeLoop: return "random-fade-loop";
    }
    return "unknown";
}

}  // namespace nevergone::game_scene_type0_post_init
