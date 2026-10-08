#include "choose_hero_thunder_scheduler.h"

#include <algorithm>
#include <cmath>

namespace nevergone::choose_hero_thunder_scheduler {
namespace {

constexpr float kLrand48Scale = 4.656612873077392578125e-10f;  // 2^-31
constexpr float kIndexBias = 0.01f;

int recovered_index(std::uint32_t raw, int count) {
    if (count <= 0) return -1;
    const float normalized = normalize_lrand48(raw);
    const float scaled = std::fabs(kIndexBias - normalized) * static_cast<float>(count);
    const int index = static_cast<int>(scaled);
    return std::clamp(index, 0, count - 1);
}

ThunderPlan thunder_plan(std::uint32_t delay_raw, std::uint32_t fade_raw) {
    ThunderPlan result;
    const float delay_random = normalize_lrand48(delay_raw);
    const float fade_random = normalize_lrand48(fade_raw);
    result.delay_seconds = 3.0f + delay_random * 5.0f;
    result.fade_out_seconds = static_cast<float>(static_cast<double>(fade_random) * 0.7);
    return result;
}

}  // namespace

float normalize_lrand48(std::uint32_t raw) {
    const std::uint32_t value = raw & 0x7fffffffu;
    return static_cast<float>(value) * kLrand48Scale;
}

ThunderPlan initial_plan(const InitialRandomBatch& randoms) {
    return thunder_plan(randoms.delay_raw, randoms.fade_raw);
}

ReschedulePlan reschedule_plan(
        const RescheduleRandomBatch& randoms,
        int lightning_count) {
    ReschedulePlan result;
    const int first = recovered_index(randoms.lightning_index_raw_0, lightning_count);
    const int second = recovered_index(randoms.lightning_index_raw_1, lightning_count);
    const int repeated = recovered_index(randoms.lightning_index_raw_2, lightning_count);
    result.lightning_indices = {{first, second, repeated, repeated, repeated, repeated}};
    result.thunder = thunder_plan(randoms.delay_raw, randoms.thunder_fade_raw);
    return result;
}

int thunder_sound_index(std::uint32_t raw) {
    const float normalized = normalize_lrand48(raw);
    const int index = static_cast<int>(std::fabs(normalized - kIndexBias) * 30.0f);
    return index >= 0 && index < static_cast<int>(kThunderSoundCount) ? index : -1;
}

float lightning_fade_out_seconds(std::uint32_t raw) {
    return 0.5f + normalize_lrand48(raw) * 0.3f;
}

float ground_light_fade_out_seconds(std::uint32_t raw) {
    return 0.5f + std::fabs(kIndexBias - normalize_lrand48(raw)) * 0.3f;
}

}  // namespace nevergone::choose_hero_thunder_scheduler
