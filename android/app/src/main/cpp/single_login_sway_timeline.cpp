#include "single_login_sway_timeline.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace nevergone::single_login_sway_timeline {
namespace {

constexpr std::uint64_t kMask48 = (1ULL << 48U) - 1ULL;
constexpr std::uint64_t kMultiplier = 0x5deece66dULL;
constexpr std::uint64_t kIncrement = 0xbULL;
constexpr std::size_t kInitialLightRandomCalls = 12;

std::uint64_t g_prng_state = 0;
std::array<NodeConfig, kNodeCount> g_configs{};
bool g_initialized = false;

std::uint32_t next_lrand48() {
    g_prng_state = (kMultiplier * g_prng_state + kIncrement) & kMask48;
    return static_cast<std::uint32_t>(g_prng_state >> 17U);
}

double unit_random() {
    return static_cast<double>(next_lrand48()) / 2147483648.0;
}

void initialize_configs() {
    // InitUI creates/configures the four z=5 dengguang nodes before indices
    // 11..19. Their initial action setup consumes three lrand48() values per
    // light, so advance the same srand48 stream by 12 calls first.
    for (std::size_t i = 0; i < kInitialLightRandomCalls; ++i) {
        (void)next_lrand48();
    }

    // Index 11: foreground branch 01. The second random value is consumed by
    // the common path, but this special case replaces X skew with 0 and uses a
    // third random draw for Y skew.
    {
        const double duration_random = unit_random();
        (void)unit_random();
        const double y_random = unit_random();
        g_configs[0] = NodeConfig{
            static_cast<float>(58.0 + 67.5 * duration_random),
            0.0f,
            static_cast<float>(2.0 + 2.0 * y_random),
            5};
    }

    // Indices 12..13: foreground branches 02..03.
    for (std::size_t index = 1; index < 3; ++index) {
        const double duration_random = unit_random();
        const double x_random = unit_random();
        g_configs[index] = NodeConfig{
            static_cast<float>(58.0 + 67.5 * duration_random),
            static_cast<float>(2.0 + 2.0 * x_random),
            0.0f,
            5};
    }

    // Indices 14..19: six left-side distant tree nodes. The first two random
    // draws still happen in the common branch but are overwritten by the tree
    // specialization; the third/fourth draws determine skew and duration.
    for (std::size_t index = 3; index < kNodeCount; ++index) {
        (void)unit_random();
        (void)unit_random();
        const double x_random = unit_random();
        const double duration_random = unit_random();
        g_configs[index] = NodeConfig{
            static_cast<float>(1.5 + duration_random),
            static_cast<float>(4.0 + 4.0 * x_random),
            0.0f,
            3};
    }

    g_initialized = true;
}

float interpolate(float from, float to, double t) {
    return static_cast<float>(from + (to - from) * std::clamp(t, 0.0, 1.0));
}

NodeSample sample_node(const NodeConfig& config, double scene_seconds) {
    NodeSample result;
    if (scene_seconds < 0.0 || config.duration_seconds <= 0.0f) return result;

    const double segment_position = scene_seconds / static_cast<double>(config.duration_seconds);
    const std::uint64_t segment = static_cast<std::uint64_t>(std::floor(segment_position));
    const double fraction = segment_position - static_cast<double>(segment);

    float from_x = 0.0f;
    float from_y = 0.0f;
    float to_x = config.skew_x_degrees;
    float to_y = config.skew_y_degrees;

    if (segment > 0) {
        const bool toward_negative = (segment & 1ULL) != 0ULL;
        from_x = toward_negative ? config.skew_x_degrees : -config.skew_x_degrees;
        from_y = toward_negative ? config.skew_y_degrees : -config.skew_y_degrees;
        to_x = -from_x;
        to_y = -from_y;
    }

    result.skew_x_degrees = interpolate(from_x, to_x, fraction);
    result.skew_y_degrees = interpolate(from_y, to_y, fraction);
    return result;
}

}  // namespace

void reset(std::uint64_t seed_seconds) {
    // POSIX srand48(seed): X0 = (seed << 16) + 0x330e.
    g_prng_state = ((seed_seconds & 0xffffffffULL) << 16U) | 0x330eULL;
    g_prng_state &= kMask48;
    g_configs = {};
    g_initialized = false;
}

const std::array<NodeConfig, kNodeCount>& configs() {
    if (!g_initialized) initialize_configs();
    return g_configs;
}

Sample sample(double scene_seconds) {
    Sample result;
    const auto& current = configs();
    if (scene_seconds < 0.0) return result;
    for (std::size_t index = 0; index < current.size(); ++index) {
        result.nodes[index] = sample_node(current[index], scene_seconds);
    }
    return result;
}

}  // namespace nevergone::single_login_sway_timeline
