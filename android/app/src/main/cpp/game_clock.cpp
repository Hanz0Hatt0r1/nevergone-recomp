#include "game_clock.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>

namespace nevergone::game_clock {
namespace {

using Clock = std::chrono::steady_clock;
constexpr int kMaxCatchupTicks = 5;
constexpr double kMaxFrameDeltaSeconds = 0.25;

std::mutex g_mutex;
Clock::time_point g_last_time{};
double g_accumulator = 0.0;
bool g_started = false;
bool g_paused = false;
std::atomic<std::uint64_t> g_tick_count{0};
std::atomic<std::uint64_t> g_dropped_catchup_count{0};

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_last_time = Clock::now();
    g_accumulator = 0.0;
    g_started = true;
    g_paused = false;
    g_tick_count.store(0, std::memory_order_relaxed);
    g_dropped_catchup_count.store(0, std::memory_order_relaxed);
}

void pause() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_paused = true;
}

void resume() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_last_time = Clock::now();
    g_accumulator = 0.0;
    g_started = true;
    g_paused = false;
}

int advance() {
    const Clock::time_point now = Clock::now();
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_paused) return 0;
    if (!g_started) {
        g_last_time = now;
        g_started = true;
        return 0;
    }

    double delta = std::chrono::duration<double>(now - g_last_time).count();
    g_last_time = now;
    delta = std::clamp(delta, 0.0, kMaxFrameDeltaSeconds);
    g_accumulator += delta;

    const int due_ticks = static_cast<int>(g_accumulator / kFixedStepSeconds);
    const int run_ticks = std::min(due_ticks, kMaxCatchupTicks);
    if (run_ticks > 0) {
        g_accumulator -= static_cast<double>(run_ticks) * kFixedStepSeconds;
        g_tick_count.fetch_add(static_cast<std::uint64_t>(run_ticks), std::memory_order_relaxed);
    }

    if (due_ticks > kMaxCatchupTicks) {
        const int dropped = due_ticks - kMaxCatchupTicks;
        g_accumulator -= static_cast<double>(dropped) * kFixedStepSeconds;
        g_dropped_catchup_count.fetch_add(
            static_cast<std::uint64_t>(dropped), std::memory_order_relaxed);
    }

    if (g_accumulator < 0.0) g_accumulator = 0.0;
    return run_ticks;
}

std::uint64_t tick_count() {
    return g_tick_count.load(std::memory_order_relaxed);
}

std::uint64_t dropped_catchup_count() {
    return g_dropped_catchup_count.load(std::memory_order_relaxed);
}

std::string status_report() {
    bool paused = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        paused = g_paused;
    }
    std::ostringstream out;
    out << "game clock: fixed 35 Hz (" << kFixedStepSeconds << " s)\n";
    out << "game clock state: " << (paused ? "paused" : "running") << "\n";
    out << "game ticks: " << tick_count() << "\n";
    out << "dropped catch-up ticks: " << dropped_catchup_count() << "\n";
    return out.str();
}

}  // namespace nevergone::game_clock
