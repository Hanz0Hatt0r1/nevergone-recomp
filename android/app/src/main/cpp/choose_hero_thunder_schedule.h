#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nevergone::choose_hero_thunder_schedule {

constexpr std::uint32_t kLrand48Max = 0x7fffffffu;
constexpr int kThunderSoundCount = 7;
constexpr int kInitialRandomBurnCount = 6;

struct FlashTiming {
    float delay_seconds = 0.0f;
    float fade_seconds = 0.0f;
};

// lrand48() returns a non-negative 31-bit integer. The shipped ARMv7 code
// multiplies it by 2^-31 before applying the recovered timing formulas.
float unit_from_lrand48(std::uint32_t value);

// RamodThunder/FuncThunderEnd menlei timing:
// delay = 3 + 5*u, fade-out = 0.7*u.
FlashTiming menlei_timing(
    std::uint32_t delay_random,
    std::uint32_t fade_random);

// FuncThunderEnd chooses six shandian array indices using three lrand48 calls:
// the first two indices are independent and the third is repeated four times.
std::array<int, 6> shandian_indices(
    std::uint32_t first_random,
    std::uint32_t second_random,
    std::uint32_t repeated_random,
    std::size_t count);

// Per-idle-shandian fade-out duration. FuncThunderEnd reuses the current
// menlei delay and chooses 0.5 + 0.3*u for the shandian fade.
float shandian_fade_seconds(std::uint32_t random_value);

// Ground-light fade-out duration after the same menlei delay:
// 0.5 + 0.3*abs(0.01-u).
float ground_light_fade_seconds(std::uint32_t random_value);

// FuncThunderBen maps lrand48 to one of seven thunder sounds only when the
// recovered integer slot is <= 6. Returns -1 when that callback is silent.
int thunder_sound_slot(std::uint32_t random_value);
const char* thunder_sound_path(int slot);

}  // namespace nevergone::choose_hero_thunder_schedule
