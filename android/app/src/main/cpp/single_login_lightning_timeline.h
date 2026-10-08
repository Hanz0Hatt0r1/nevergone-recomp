#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nevergone::single_login_lightning_timeline {

constexpr std::size_t kLightningCount = 7;
constexpr std::size_t kIlluminationCount = 4;

struct StrikeConfig {
    std::array<int, kIlluminationCount> illumination_indices{};
    double delay_seconds = 0.0;
    double fade_seconds = 0.0;
};

struct StrikeSample {
    float alpha = 0.0f;
    bool onset = false;
    int thunder_sound_index = -1;
    StrikeConfig config{};
};

struct Sample {
    std::array<StrikeSample, kLightningCount> strikes{};
};

// Replays the shared srand48/lrand48 stream up to the lightning block.
// The original InitUI consumes 43 values first: 12 for dengguang and 31 for
// foreground/tree sway setup.
void reset(std::uint64_t seed_seconds);

// Stateful chronological sample. Calls should use nondecreasing scene time;
// rewinding is supported by resetting/replaying from the same seed.
Sample sample(double scene_seconds);

}  // namespace nevergone::single_login_lightning_timeline
