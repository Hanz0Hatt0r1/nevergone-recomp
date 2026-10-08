#pragma once

#include <array>
#include <cstdint>

namespace nevergone::choose_hero_thunder_scheduler {

constexpr std::size_t kThunderSpriteCount = 6;
constexpr std::size_t kLightningSelectionCount = 6;
constexpr std::size_t kThunderSoundCount = 7;

struct InitialRandomBatch {
    // RamodThunder performs six lrand48 calls whose results are overwritten by
    // intervening CCArray::count calls. They still advance the global RNG and
    // therefore remain part of the shipped random stream contract.
    std::array<std::uint32_t, 6> discarded{};
    std::uint32_t delay_raw = 0;
    std::uint32_t fade_raw = 0;
};

struct ThunderPlan {
    float delay_seconds = 0.0f;
    float fade_out_seconds = 0.0f;
};

struct RescheduleRandomBatch {
    std::uint32_t lightning_index_raw_0 = 0;
    std::uint32_t lightning_index_raw_1 = 0;
    std::uint32_t lightning_index_raw_2 = 0;
    std::uint32_t delay_raw = 0;
    std::uint32_t thunder_fade_raw = 0;
};

struct ReschedulePlan {
    std::array<int, kLightningSelectionCount> lightning_indices{};
    ThunderPlan thunder;
};

float normalize_lrand48(std::uint32_t raw);
ThunderPlan initial_plan(const InitialRandomBatch& randoms);
ReschedulePlan reschedule_plan(
    const RescheduleRandomBatch& randoms,
    int lightning_count = static_cast<int>(kThunderSpriteCount));

// FuncThunderBen chooses from seven imported thunder effects. A value outside
// [0,6] means the callback intentionally plays no sound for that flash.
int thunder_sound_index(std::uint32_t raw);

// Durations consumed by FuncThunderEnd when a selected lightning/ground-light
// node is idle and therefore receives a new action sequence.
float lightning_fade_out_seconds(std::uint32_t raw);
float ground_light_fade_out_seconds(std::uint32_t raw);

}  // namespace nevergone::choose_hero_thunder_scheduler
