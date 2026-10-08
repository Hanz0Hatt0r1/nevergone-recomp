#include "single_login_light_timeline.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace nevergone::single_login_light_timeline {
namespace {

constexpr std::uint64_t kMask48 = (1ULL << 48U) - 1ULL;
constexpr std::uint64_t kMultiplier = 0x5deece66dULL;
constexpr std::uint64_t kIncrement = 0xbULL;
constexpr double kFadeSeconds = 2.0;
constexpr double kInitialDelaySeconds = 2.0;

struct LayerState {
    double cycle_start = 0.0;
    double hold_seconds = 0.0;
    bool initial_cycle = true;
};

std::uint64_t g_prng_state = 0;
std::array<LayerState, kLightCount> g_layers{};
bool g_scene_initialized = false;

std::uint32_t next_lrand48() {
    g_prng_state = (kMultiplier * g_prng_state + kIncrement) & kMask48;
    return static_cast<std::uint32_t>(g_prng_state >> 17U);
}

double unit_random() {
    return static_cast<double>(next_lrand48()) / 2147483648.0;
}

double initial_hold() {
    // InitUI consumes three lrand48() values per light before using the third
    // one for 5 + random*10 seconds.
    (void)next_lrand48();
    (void)next_lrand48();
    return 5.0 + unit_random() * 10.0;
}

double recurring_hold() {
    // Func02 consumes one lrand48() and uses 5 + random*5 seconds.
    return 5.0 + unit_random() * 5.0;
}

double cycle_duration(const LayerState& layer) {
    return (layer.initial_cycle ? kInitialDelaySeconds : 0.0) +
        kFadeSeconds + layer.hold_seconds + kFadeSeconds;
}

void initialize_scene() {
    for (auto& layer : g_layers) {
        layer.cycle_start = 0.0;
        layer.hold_seconds = initial_hold();
        layer.initial_cycle = true;
    }
    g_scene_initialized = true;
}

void advance_to(double scene_seconds) {
    // Original callbacks share one lrand48 state. Advance whichever layer's
    // callback would fire first so PRNG consumption follows callback time,
    // rather than renderer iteration order.
    while (true) {
        std::size_t next_index = kLightCount;
        double next_end = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index < g_layers.size(); ++index) {
            const auto& layer = g_layers[index];
            const double end = layer.cycle_start + cycle_duration(layer);
            if (end < next_end) {
                next_end = end;
                next_index = index;
            }
        }
        if (next_index >= g_layers.size() || next_end > scene_seconds) {
            return;
        }

        auto& layer = g_layers[next_index];
        layer.cycle_start = next_end;
        layer.initial_cycle = false;
        layer.hold_seconds = recurring_hold();
    }
}

float alpha_for(const LayerState& layer, double scene_seconds) {
    double local = scene_seconds - layer.cycle_start;
    if (local < 0.0) return 0.0f;

    if (layer.initial_cycle) {
        if (local < kInitialDelaySeconds) return 0.0f;
        local -= kInitialDelaySeconds;
    }

    if (local < kFadeSeconds) {
        return static_cast<float>(std::clamp(local / kFadeSeconds, 0.0, 1.0));
    }
    local -= kFadeSeconds;

    if (local < layer.hold_seconds) return 1.0f;
    local -= layer.hold_seconds;

    if (local < kFadeSeconds) {
        return static_cast<float>(std::clamp(1.0 - local / kFadeSeconds, 0.0, 1.0));
    }
    return 0.0f;
}

}  // namespace

void reset(std::uint64_t seed_seconds) {
    // POSIX srand48(seed): X0 = (seed << 16) + 0x330e.
    g_prng_state = ((seed_seconds & 0xffffffffULL) << 16U) | 0x330eULL;
    g_prng_state &= kMask48;
    g_scene_initialized = false;
    g_layers = {};
}

Sample sample(double scene_seconds) {
    Sample result;
    if (scene_seconds < 0.0) return result;
    if (!g_scene_initialized) initialize_scene();
    advance_to(scene_seconds);
    for (std::size_t index = 0; index < g_layers.size(); ++index) {
        result.alpha[index] = alpha_for(g_layers[index], scene_seconds);
    }
    return result;
}

}  // namespace nevergone::single_login_light_timeline
