#include "single_login_lightning_timeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace nevergone::single_login_lightning_timeline {
namespace {

constexpr std::uint64_t kMask48 = (1ULL << 48U) - 1ULL;
constexpr std::uint64_t kMultiplier = 0x5deece66dULL;
constexpr std::uint64_t kIncrement = 0xbULL;
constexpr std::size_t kPriorInitRandomCalls = 43;

struct StrikeState {
    StrikeConfig config{};
    double cycle_start = 0.0;
    double onset_time = 0.0;
    double end_time = 0.0;
    bool onset_consumed = false;
    int thunder_sound_index = -1;
};

std::uint64_t g_seed_seconds = 0;
std::uint64_t g_prng_state = 0;
std::array<StrikeState, kLightningCount> g_strikes{};
bool g_initialized = false;
double g_last_sample_seconds = -1.0;

std::uint32_t next_lrand48() {
    g_prng_state = (kMultiplier * g_prng_state + kIncrement) & kMask48;
    return static_cast<std::uint32_t>(g_prng_state >> 17U);
}

double unit_random() {
    return static_cast<double>(next_lrand48()) / 2147483648.0;
}

int illumination_index() {
    // Original InitUI/Func01 uses abs(0.01 - lrand48()/2^31) * count.
    // With count=4 this always lands in [0,3].
    const double scaled = std::fabs(0.01 - unit_random()) *
        static_cast<double>(kIlluminationCount);
    return std::clamp(static_cast<int>(scaled), 0,
        static_cast<int>(kIlluminationCount) - 1);
}

StrikeConfig next_cycle_config() {
    StrikeConfig config;
    for (auto& index : config.illumination_indices) index = illumination_index();
    config.delay_seconds = 3.0 + 5.0 * unit_random();
    config.fade_seconds = 1.8 * unit_random();
    return config;
}

int next_thunder_sound() {
    // FuncBegin maps one lrand48 draw to 0..29 and only plays 0..6.
    const int candidate = static_cast<int>(unit_random() * 30.0);
    return candidate >= 0 && candidate <= 6 ? candidate : -1;
}

void configure_cycle(StrikeState* strike, double cycle_start) {
    strike->config = next_cycle_config();
    strike->cycle_start = cycle_start;
    strike->onset_time = cycle_start + strike->config.delay_seconds;
    strike->end_time = strike->onset_time + strike->config.fade_seconds;
    strike->onset_consumed = false;
    strike->thunder_sound_index = -1;
}

void initialize_scene() {
    for (std::size_t i = 0; i < kPriorInitRandomCalls; ++i) (void)next_lrand48();
    for (auto& strike : g_strikes) configure_cycle(&strike, 0.0);
    g_initialized = true;
}

enum class EventType { Onset, End };

struct Event {
    double time = std::numeric_limits<double>::infinity();
    std::size_t index = kLightningCount;
    EventType type = EventType::Onset;
};

Event next_event() {
    Event result;
    for (std::size_t index = 0; index < g_strikes.size(); ++index) {
        const auto& strike = g_strikes[index];
        if (!strike.onset_consumed && strike.onset_time < result.time) {
            result = {strike.onset_time, index, EventType::Onset};
        }
        if (strike.onset_consumed && strike.end_time < result.time) {
            result = {strike.end_time, index, EventType::End};
        }
    }
    return result;
}

void advance_to(double scene_seconds) {
    while (true) {
        const Event event = next_event();
        if (event.index >= kLightningCount || event.time > scene_seconds) return;
        auto& strike = g_strikes[event.index];
        if (event.type == EventType::Onset) {
            strike.thunder_sound_index = next_thunder_sound();
            strike.onset_consumed = true;
        } else {
            // End callback is Func01, which immediately consumes six random
            // values to schedule the next cycle for this lightning node.
            configure_cycle(&strike, event.time);
        }
    }
}

float alpha_for(const StrikeState& strike, double scene_seconds) {
    if (scene_seconds < strike.onset_time || !strike.onset_consumed) return 0.0f;
    if (strike.config.fade_seconds <= 0.0) return 0.0f;
    if (scene_seconds >= strike.end_time) return 0.0f;
    return static_cast<float>(std::clamp(
        1.0 - (scene_seconds - strike.onset_time) / strike.config.fade_seconds,
        0.0,
        1.0));
}

void rewind_and_replay(double scene_seconds) {
    reset(g_seed_seconds);
    initialize_scene();
    advance_to(scene_seconds);
}

}  // namespace

void reset(std::uint64_t seed_seconds) {
    g_seed_seconds = seed_seconds;
    g_prng_state = ((seed_seconds & 0xffffffffULL) << 16U) | 0x330eULL;
    g_prng_state &= kMask48;
    g_strikes = {};
    g_initialized = false;
    g_last_sample_seconds = -1.0;
}

Sample sample(double scene_seconds) {
    Sample result;
    if (scene_seconds < 0.0) return result;
    if (!g_initialized) initialize_scene();
    if (scene_seconds < g_last_sample_seconds) {
        rewind_and_replay(scene_seconds);
    } else {
        advance_to(scene_seconds);
    }
    g_last_sample_seconds = scene_seconds;

    for (std::size_t index = 0; index < g_strikes.size(); ++index) {
        const auto& strike = g_strikes[index];
        auto& out = result.strikes[index];
        out.alpha = alpha_for(strike, scene_seconds);
        out.onset = strike.onset_consumed && scene_seconds >= strike.onset_time &&
            scene_seconds < strike.end_time;
        out.thunder_sound_index = strike.thunder_sound_index;
        out.config = strike.config;
    }
    return result;
}

}  // namespace nevergone::single_login_lightning_timeline
