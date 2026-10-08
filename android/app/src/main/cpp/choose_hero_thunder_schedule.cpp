#include "choose_hero_thunder_schedule.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace nevergone::choose_hero_thunder_schedule {
namespace {

constexpr float kLrand48Scale = 0x1p-31f;
constexpr std::array<const char*, kThunderSoundCount> kThunderSounds{{
    "SingleLogin_UI/L_thunder08-r.mp3",
    "SingleLogin_UI/L_Thunder09.mp3",
    "SingleLogin_UI/L_Thunder10.mp3",
    "SingleLogin_UI/M_thunder_norm_2.mp3",
    "SingleLogin_UI/M_thunder_norm_5.mp3",
    "SingleLogin_UI/M_Thunder04.mp3",
    "SingleLogin_UI/S_thunder_norm_1.mp3",
}};

int sampled_array_index(std::uint32_t random_value, std::size_t count) {
    if (count == 0) return -1;
    const float unit = unit_from_lrand48(random_value);
    const float scaled = std::fabs(0.01f - unit) * static_cast<float>(count);
    int index = static_cast<int>(scaled);  // ARM VCVT.S32.F32 truncates here.
    if (index < 0) index = 0;
    if (static_cast<std::size_t>(index) >= count) {
        index = static_cast<int>(count - 1);
    }
    return index;
}

}  // namespace

float unit_from_lrand48(std::uint32_t value) {
    value &= kLrand48Max;
    // Match the shipped conversion order: integer -> float, then multiply by
    // the exact 2^-31 single-precision constant.
    return static_cast<float>(value) * kLrand48Scale;
}

FlashTiming menlei_timing(
        std::uint32_t delay_random,
        std::uint32_t fade_random) {
    FlashTiming result;
    result.delay_seconds = 3.0f + unit_from_lrand48(delay_random) * 5.0f;
    const float fade_unit = unit_from_lrand48(fade_random);
    result.fade_seconds = static_cast<float>(static_cast<double>(fade_unit) * 0.7);
    return result;
}

std::array<int, 6> shandian_indices(
        std::uint32_t first_random,
        std::uint32_t second_random,
        std::uint32_t repeated_random,
        std::size_t count) {
    const int first = sampled_array_index(first_random, count);
    const int second = sampled_array_index(second_random, count);
    const int repeated = sampled_array_index(repeated_random, count);
    return {{first, second, repeated, repeated, repeated, repeated}};
}

float shandian_fade_seconds(std::uint32_t random_value) {
    return 0.5f + unit_from_lrand48(random_value) * 0.3f;
}

float ground_light_fade_seconds(std::uint32_t random_value) {
    const float delta = std::fabs(0.01f - unit_from_lrand48(random_value));
    return 0.5f + delta * 0.3f;
}

int thunder_sound_slot(std::uint32_t random_value) {
    const float unit = unit_from_lrand48(random_value);
    const int slot = static_cast<int>(std::fabs(unit - 0.01f) * 30.0f);
    return slot >= 0 && slot < kThunderSoundCount ? slot : -1;
}

const char* thunder_sound_path(int slot) {
    if (slot < 0 || slot >= kThunderSoundCount) return nullptr;
    return kThunderSounds[static_cast<std::size_t>(slot)];
}

}  // namespace nevergone::choose_hero_thunder_schedule
