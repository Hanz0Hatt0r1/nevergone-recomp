#include <cmath>
#include <iostream>

#include "single_login_sway_timeline.h"

namespace {

bool near(float actual, float expected, float tolerance = 1.0e-4f) {
    return std::fabs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    using namespace nevergone::single_login_sway_timeline;

    reset(1234);
    const auto first = configs();
    if (first.size() != kNodeCount) {
        std::cerr << "node count mismatch\n";
        return 1;
    }

    // Fixed seed values pin the recovered srand48/lrand48 call ordering,
    // including the 12 draws consumed by the four light nodes first.
    if (!near(first[0].duration_seconds, 83.32694f) ||
        !near(first[0].skew_x_degrees, 0.0f) ||
        !near(first[0].skew_y_degrees, 2.3785657f) ||
        first[0].z_order != 5 ||
        !near(first[1].duration_seconds, 106.00329f) ||
        !near(first[1].skew_x_degrees, 3.0579872f) ||
        !near(first[2].duration_seconds, 79.79499f) ||
        !near(first[2].skew_x_degrees, 3.9280639f) ||
        !near(first[3].duration_seconds, 2.3509564f) ||
        !near(first[3].skew_x_degrees, 6.7090597f) ||
        first[3].z_order != 3) {
        std::cerr << "recovered parameter mismatch\n";
        return 1;
    }

    for (std::size_t index = 0; index < kNodeCount; ++index) {
        const auto& config = first[index];
        if (index < 3) {
            if (config.duration_seconds < 58.0f || config.duration_seconds > 125.5f ||
                config.z_order != 5) {
                std::cerr << "foreground parameter bounds mismatch\n";
                return 1;
            }
        } else {
            if (config.duration_seconds < 1.5f || config.duration_seconds > 2.5f ||
                config.skew_x_degrees < 4.0f || config.skew_x_degrees > 8.0f ||
                config.z_order != 3) {
                std::cerr << "tree parameter bounds mismatch\n";
                return 1;
            }
        }
    }

    // CCSkewTo starts from zero, reaches +amplitude after one duration,
    // reaches -amplitude after two durations, then repeats between +/-.
    const auto config = first[3];
    const auto at_zero = sample(0.0).nodes[3];
    const auto at_half = sample(config.duration_seconds * 0.5).nodes[3];
    const auto at_one = sample(config.duration_seconds).nodes[3];
    const auto at_two = sample(config.duration_seconds * 2.0).nodes[3];
    const auto at_three = sample(config.duration_seconds * 3.0).nodes[3];
    if (!near(at_zero.skew_x_degrees, 0.0f) ||
        !near(at_half.skew_x_degrees, config.skew_x_degrees * 0.5f) ||
        !near(at_one.skew_x_degrees, config.skew_x_degrees) ||
        !near(at_two.skew_x_degrees, -config.skew_x_degrees) ||
        !near(at_three.skew_x_degrees, config.skew_x_degrees)) {
        std::cerr << "skew cycle interpolation mismatch\n";
        return 1;
    }

    reset(1234);
    const auto same = configs();
    reset(4321);
    const auto different = configs();
    if (!near(same[8].duration_seconds, first[8].duration_seconds) ||
        !near(same[8].skew_x_degrees, first[8].skew_x_degrees) ||
        (near(different[0].duration_seconds, first[0].duration_seconds) &&
         near(different[3].skew_x_degrees, first[3].skew_x_degrees))) {
        std::cerr << "seed determinism mismatch\n";
        return 1;
    }

    std::cout << "SingleLogin sway timeline smoke OK\n";
    return 0;
}
