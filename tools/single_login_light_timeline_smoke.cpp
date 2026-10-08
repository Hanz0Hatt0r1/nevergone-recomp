#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "single_login_light_timeline.h"

namespace {

bool nearly(float actual, float expected, float tolerance = 1.0e-5f) {
    return std::fabs(actual - expected) <= tolerance;
}

bool all_equal(const nevergone::single_login_light_timeline::Sample& sample, float expected) {
    for (float alpha : sample.alpha) {
        if (!nearly(alpha, expected)) return false;
    }
    return true;
}

bool bounded(const nevergone::single_login_light_timeline::Sample& sample) {
    for (float alpha : sample.alpha) {
        if (!(alpha >= 0.0f && alpha <= 1.0f)) return false;
    }
    return true;
}

}  // namespace

int main() {
    using nevergone::single_login_light_timeline::reset;
    using nevergone::single_login_light_timeline::sample;

    constexpr std::uint64_t kSeed = 1'700'000'000ULL;
    reset(kSeed);

    if (!all_equal(sample(-0.1), 0.0f) ||
        !all_equal(sample(0.0), 0.0f) ||
        !all_equal(sample(1.999), 0.0f)) {
        std::cerr << "initial two-second delay mismatch\n";
        return 1;
    }

    if (!all_equal(sample(3.0), 0.5f) || !all_equal(sample(4.0), 1.0f)) {
        std::cerr << "initial two-second fade-in mismatch\n";
        return 1;
    }

    const std::array<double, 8> probe_times = {
        4.0, 7.0, 11.0, 15.0, 20.0, 30.0, 60.0, 180.0
    };
    std::array<nevergone::single_login_light_timeline::Sample, probe_times.size()> first{};
    for (std::size_t index = 0; index < probe_times.size(); ++index) {
        first[index] = sample(probe_times[index]);
        if (!bounded(first[index])) {
            std::cerr << "alpha escaped [0,1] at " << probe_times[index] << "s\n";
            return 1;
        }
    }

    reset(kSeed);
    for (std::size_t index = 0; index < probe_times.size(); ++index) {
        const auto second = sample(probe_times[index]);
        for (std::size_t light = 0; light < second.alpha.size(); ++light) {
            if (!nearly(second.alpha[light], first[index].alpha[light])) {
                std::cerr << "timeline is not deterministic after reset at "
                          << probe_times[index] << "s, light " << light << "\n";
                return 1;
            }
        }
    }

    reset(kSeed + 1);
    const auto different_seed = sample(30.0);
    bool differs = false;
    for (std::size_t light = 0; light < different_seed.alpha.size(); ++light) {
        if (!nearly(different_seed.alpha[light], first[5].alpha[light])) {
            differs = true;
            break;
        }
    }
    if (!differs) {
        std::cerr << "different srand48-equivalent seeds produced identical 30s sample\n";
        return 1;
    }

    std::cout << "SingleLogin light timeline smoke OK\n";
    return 0;
}
