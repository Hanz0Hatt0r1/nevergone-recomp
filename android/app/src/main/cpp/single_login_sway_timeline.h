#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nevergone::single_login_sway_timeline {

constexpr std::size_t kNodeCount = 9;

struct NodeConfig {
    float duration_seconds = 0.0f;
    float skew_x_degrees = 0.0f;
    float skew_y_degrees = 0.0f;
    int z_order = 0;
};

struct NodeSample {
    float skew_x_degrees = 0.0f;
    float skew_y_degrees = 0.0f;
};

struct Sample {
    std::array<NodeSample, kNodeCount> nodes{};
};

// Rebuild the InitUI-time random parameters. The seed follows POSIX
// srand48(time(NULL)) semantics used by the original startup path.
void reset(std::uint64_t seed_seconds);

const std::array<NodeConfig, kNodeCount>& configs();
Sample sample(double scene_seconds);

}  // namespace nevergone::single_login_sway_timeline
