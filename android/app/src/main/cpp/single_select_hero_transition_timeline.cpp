#include "single_select_hero_transition_timeline.h"

#include <limits>
#include <mutex>
#include <sstream>

namespace nevergone::single_select_hero_transition_timeline {
namespace {

std::mutex g_mutex;
Snapshot g_state;

std::uint64_t saturating_add(std::uint64_t lhs, std::uint64_t rhs) {
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return lhs + rhs;
}

void begin_locked(
        Phase phase,
        std::uint64_t tick,
        std::uint64_t selector_generation,
        std::uint64_t duration_ticks,
        std::int64_t old_career,
        std::int64_t new_career) {
    const std::uint64_t prior_begin_count = g_state.begin_count;
    const std::uint64_t prior_completion_count = g_state.completion_count;
    const std::uint64_t prior_stale_count = g_state.stale_generation_count;
    g_state = Snapshot{};
    g_state.phase = phase;
    g_state.selector_generation = selector_generation;
    g_state.begin_tick = tick;
    g_state.unlock_tick = saturating_add(tick, duration_ticks);
    g_state.old_career = old_career;
    g_state.new_career = new_career;
    g_state.begin_count = prior_begin_count + 1;
    g_state.completion_count = prior_completion_count;
    g_state.stale_generation_count = prior_stale_count;
}

}  // namespace

bool valid_career(std::int64_t career) {
    return career >= 1 && career <= 5;
}

std::uint64_t career_change_unlock_ticks(
        std::int64_t old_career,
        std::int64_t new_career) {
    if (!valid_career(old_career) || !valid_career(new_career) ||
            old_career == new_career) {
        return 0;
    }
    const std::int64_t distance = old_career > new_career
        ? old_career - new_career
        : new_career - old_career;
    return distance <= 2
        ? kNearCareerChangeUnlockTicks
        : kFarCareerChangeUnlockTicks;
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = Snapshot{};
}

void begin_initial(
        std::uint64_t tick,
        std::uint64_t selector_generation,
        std::int64_t selected_career) {
    if (selector_generation == 0 || !valid_career(selected_career)) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    begin_locked(
        Phase::kInitialCarousel,
        tick,
        selector_generation,
        kInitialUnlockTicks,
        selected_career,
        selected_career);
}

bool begin_career_change(
        std::uint64_t tick,
        std::uint64_t selector_generation,
        std::int64_t old_career,
        std::int64_t new_career) {
    const std::uint64_t duration = career_change_unlock_ticks(old_career, new_career);
    if (selector_generation == 0 || duration == 0) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    begin_locked(
        Phase::kCareerChange,
        tick,
        selector_generation,
        duration,
        old_career,
        new_career);
    return true;
}

bool advance(std::uint64_t tick, std::uint64_t selector_generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.phase == Phase::kInactive) return false;

    if (selector_generation == 0 || selector_generation != g_state.selector_generation) {
        const std::uint64_t begin_count = g_state.begin_count;
        const std::uint64_t completion_count = g_state.completion_count;
        const std::uint64_t stale_count = g_state.stale_generation_count + 1;
        g_state = Snapshot{};
        g_state.begin_count = begin_count;
        g_state.completion_count = completion_count;
        g_state.stale_generation_count = stale_count;
        return false;
    }

    if (tick < g_state.unlock_tick) return false;

    ++g_state.completion_count;
    g_state.phase = Phase::kInactive;
    g_state.selector_generation = 0;
    g_state.begin_tick = 0;
    g_state.unlock_tick = 0;
    g_state.old_career = 0;
    g_state.new_career = 0;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* phase_name(Phase phase) {
    switch (phase) {
        case Phase::kInactive: return "inactive";
        case Phase::kInitialCarousel: return "initial-carousel";
        case Phase::kCareerChange: return "career-change";
    }
    return "unknown";
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream out;
    out << "single select transition: " << phase_name(g_state.phase)
        << " generation=" << g_state.selector_generation
        << " old-career=" << g_state.old_career
        << " new-career=" << g_state.new_career
        << " begin-tick=" << g_state.begin_tick
        << " unlock-tick=" << g_state.unlock_tick << "\n";
    out << "single select transition counts: begin=" << g_state.begin_count
        << " complete=" << g_state.completion_count
        << " stale=" << g_state.stale_generation_count << "\n";
    return out.str();
}

}  // namespace nevergone::single_select_hero_transition_timeline
