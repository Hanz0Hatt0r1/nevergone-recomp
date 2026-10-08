#include <cmath>
#include <cstdint>
#include <iostream>

#include "game_clock.h"
#include "splash_sequence_state.h"
#include "splash_timeline.h"

namespace {

constexpr double kEpsilon = 1.0e-9;

bool nearly(double actual, double expected, double tolerance = kEpsilon) {
    return std::fabs(actual - expected) <= tolerance;
}

std::uint64_t ticks_for(double seconds) {
    return static_cast<std::uint64_t>(
        std::ceil(seconds / nevergone::game_clock::kFixedStepSeconds - kEpsilon));
}

}  // namespace

int main() {
    using namespace nevergone;

    splash_sequence_state::reset();
    if (splash_sequence_state::started() ||
        splash_sequence_state::complete(1000) ||
        splash_sequence_state::elapsed_tick(1000) != 0 ||
        splash_sequence_state::single_login_seconds(1000) >= 0.0) {
        std::cerr << "reset state mismatch\n";
        return 1;
    }

    const std::uint64_t generation0 = splash_sequence_state::generation();
    constexpr std::uint64_t kStartA = 1000;
    splash_sequence_state::begin(kStartA);
    if (!splash_sequence_state::started() ||
        splash_sequence_state::generation() != generation0 + 1 ||
        splash_sequence_state::elapsed_tick(kStartA) != 0 ||
        splash_sequence_state::elapsed_tick(kStartA - 1) != 0) {
        std::cerr << "begin state mismatch\n";
        return 1;
    }

    const std::uint64_t sound_tick = ticks_for(splash_timeline::kStartupSoundSeconds);
    const double sound_seconds = static_cast<double>(sound_tick) * game_clock::kFixedStepSeconds;
    if (sound_seconds + kEpsilon < splash_timeline::kStartupSoundSeconds ||
        splash_sequence_state::complete(kStartA + sound_tick)) {
        std::cerr << "startup sound cue window mismatch\n";
        return 1;
    }

    const std::uint64_t complete_tick = ticks_for(splash_timeline::kTimelineCompleteSeconds);
    if (splash_sequence_state::complete(kStartA + complete_tick - 1)) {
        std::cerr << "sequence completed one tick too early\n";
        return 1;
    }
    if (!splash_sequence_state::complete(kStartA + complete_tick)) {
        std::cerr << "sequence did not complete at recovered boundary\n";
        return 1;
    }
    if (!nearly(
            splash_sequence_state::single_login_seconds(kStartA + complete_tick),
            static_cast<double>(complete_tick) * game_clock::kFixedStepSeconds -
                splash_timeline::kTimelineCompleteSeconds,
            1.0e-6)) {
        std::cerr << "SingleLogin local zero mismatch\n";
        return 1;
    }

    // Hot re-import must create a fresh local origin even if global uptime is high.
    splash_sequence_state::reset();
    constexpr std::uint64_t kStartB = 250000;
    splash_sequence_state::begin(kStartB);
    if (splash_sequence_state::generation() != generation0 + 2 ||
        splash_sequence_state::elapsed_tick(kStartB) != 0 ||
        splash_sequence_state::complete(kStartB + sound_tick)) {
        std::cerr << "hot-restart sequence origin mismatch\n";
        return 1;
    }

    if (!splash_sequence_state::complete(kStartB + complete_tick)) {
        std::cerr << "hot-restart completion mismatch\n";
        return 1;
    }

    std::cout << "Splash sequence state smoke OK\n";
    return 0;
}
