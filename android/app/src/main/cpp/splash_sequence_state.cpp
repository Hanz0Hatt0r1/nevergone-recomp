#include "splash_sequence_state.h"

#include <atomic>

#include "game_clock.h"
#include "initial_ui_transition.h"
#include "splash_timeline.h"

namespace nevergone::splash_sequence_state {
namespace {

std::atomic<bool> g_started{false};
std::atomic<std::uint64_t> g_start_tick{0};
std::atomic<std::uint64_t> g_generation{0};

}  // namespace

void reset() {
    g_started.store(false, std::memory_order_release);
    g_start_tick.store(0, std::memory_order_relaxed);
}

void begin(std::uint64_t start_tick) {
    g_start_tick.store(start_tick, std::memory_order_relaxed);
    g_generation.fetch_add(1, std::memory_order_relaxed);
    g_started.store(true, std::memory_order_release);
}

bool started() {
    return g_started.load(std::memory_order_acquire);
}

std::uint64_t generation() {
    return g_generation.load(std::memory_order_relaxed);
}

std::uint64_t elapsed_tick(std::uint64_t now_tick) {
    if (!started()) return 0;
    const std::uint64_t start = g_start_tick.load(std::memory_order_relaxed);
    return now_tick >= start ? now_tick - start : 0;
}

bool complete(std::uint64_t now_tick) {
    return started() && splash_timeline::sample_tick(elapsed_tick(now_tick)).complete;
}

double single_login_seconds(std::uint64_t now_tick) {
    if (!complete(now_tick)) return -1.0;
    const auto ui_state = initial_ui_transition::snapshot();
    if (ui_state.phase != initial_ui_transition::Phase::kManagementLoginInitialized ||
        ui_state.scene_generation != generation()) {
        return -1.0;
    }
    return static_cast<double>(elapsed_tick(now_tick)) * game_clock::kFixedStepSeconds -
        splash_timeline::kTimelineCompleteSeconds;
}

}  // namespace nevergone::splash_sequence_state
