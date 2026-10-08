#include <cmath>
#include <iostream>

#include "single_login_lightning_timeline.h"

namespace {

bool near(double actual, double expected, double tolerance = 1.0e-6) {
    return std::fabs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    using namespace nevergone::single_login_lightning_timeline;

    reset(1234);
    const auto initial = sample(0.0);
    const auto& first = initial.strikes[0].config;
    const auto& sixth = initial.strikes[5].config;
    const auto& seventh = initial.strikes[6].config;

    if (first.illumination_indices != std::array<int, 4>{2, 3, 1, 2} ||
        !near(first.delay_seconds, 7.3793454091) ||
        !near(first.fade_seconds, 0.2624589935) ||
        sixth.illumination_indices != std::array<int, 4>{0, 3, 2, 0} ||
        !near(sixth.delay_seconds, 3.2688796865) ||
        seventh.illumination_indices != std::array<int, 4>{2, 2, 2, 0} ||
        !near(seventh.delay_seconds, 3.0832035602) ||
        !near(seventh.fade_seconds, 1.4973919124)) {
        std::cerr << "initial recovered lightning configuration mismatch\n";
        return 1;
    }

    const auto before = sample(seventh.delay_seconds - 1.0e-4);
    if (before.strikes[6].alpha != 0.0f || before.strikes[6].onset) {
        std::cerr << "lightning became visible before onset\n";
        return 1;
    }

    const auto at_onset = sample(seventh.delay_seconds);
    if (at_onset.strikes[6].alpha < 0.999f || !at_onset.strikes[6].onset) {
        std::cerr << "lightning onset mismatch\n";
        return 1;
    }

    const auto midway = sample(seventh.delay_seconds + seventh.fade_seconds * 0.5);
    if (midway.strikes[6].alpha < 0.49f || midway.strikes[6].alpha > 0.51f) {
        std::cerr << "lightning fade interpolation mismatch\n";
        return 1;
    }

    // Long-run sampling exercises chronological onset/end callbacks and the
    // six PRNG draws consumed by every Func01 restart.
    for (double seconds = 0.0; seconds <= 120.0; seconds += 0.125) {
        const auto current = sample(seconds);
        for (const auto& strike : current.strikes) {
            if (strike.alpha < 0.0f || strike.alpha > 1.0f ||
                strike.config.delay_seconds < 3.0 || strike.config.delay_seconds > 8.0 ||
                strike.config.fade_seconds < 0.0 || strike.config.fade_seconds > 1.8) {
                std::cerr << "long-run lightning bounds mismatch\n";
                return 1;
            }
            for (int illumination : strike.config.illumination_indices) {
                if (illumination < 0 || illumination >= 4) {
                    std::cerr << "illumination index out of bounds\n";
                    return 1;
                }
            }
        }
    }

    // Rewind support must deterministically replay the same shared PRNG stream.
    const auto late = sample(40.0);
    const auto rewound = sample(0.0);
    const auto replayed = sample(40.0);
    for (std::size_t index = 0; index < kLightningCount; ++index) {
        if (!near(late.strikes[index].alpha, replayed.strikes[index].alpha) ||
            late.strikes[index].thunder_sound_index != replayed.strikes[index].thunder_sound_index ||
            !near(late.strikes[index].config.delay_seconds, replayed.strikes[index].config.delay_seconds) ||
            !near(late.strikes[index].config.fade_seconds, replayed.strikes[index].config.fade_seconds)) {
            std::cerr << "deterministic rewind mismatch\n";
            return 1;
        }
    }
    (void)rewound;

    std::cout << "SingleLogin lightning timeline smoke OK\n";
    return 0;
}
