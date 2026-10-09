#include "single_select_hero_transition_state.h"

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <sstream>

#include "game_clock.h"
#include "single_select_hero_state.h"

namespace nevergone::single_select_hero_transition_state {
namespace {

std::mutex g_mutex;
Snapshot g_state;

bool same_transition(const single_select_hero_state::Snapshot& selector) {
    return g_state.active &&
        g_state.selector_generation == selector.generation &&
        g_state.selector_selection_count == selector.selection_count &&
        g_state.from_career == selector.transition_from_career &&
        g_state.to_career == selector.selected_career;
}

void start_transition(
        const single_select_hero_state::Snapshot& selector,
        std::uint64_t tick) {
    const std::uint64_t completions = g_state.completion_count;
    g_state = {};
    g_state.active = true;
    g_state.selector_generation = selector.generation;
    g_state.selector_selection_count = selector.selection_count;
    g_state.from_career = selector.transition_from_career;
    g_state.to_career = selector.selected_career;
    g_state.includes_door_close = selector.selection_count > 0;
    g_state.start_tick = tick;
    g_state.required_seconds = transition_seconds(
        g_state.from_career,
        g_state.to_career,
        g_state.includes_door_close);
    g_state.completion_count = completions;
}

}  // namespace

double transition_seconds(
        std::int64_t from_career,
        std::int64_t to_career,
        bool includes_door_close) {
    const std::int64_t distance = std::llabs(to_career - from_career);
    const double travel = distance <= 2
        ? kCarouselNearTravelSeconds
        : kCarouselFarTravelSeconds;
    return (includes_door_close ? kDoorCloseSeconds : 0.0) +
        travel + kCarouselOvershootSeconds + kCarouselReturnSeconds;
}

void sync(std::uint64_t tick) {
    const auto selector = single_select_hero_state::snapshot();
    bool complete = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!selector.active || !selector.transition_pending) {
            g_state.active = false;
            g_state.elapsed_seconds = 0.0;
            return;
        }

        if (!same_transition(selector)) {
            start_transition(selector, tick);
        }

        const std::uint64_t elapsed_ticks = tick >= g_state.start_tick
            ? tick - g_state.start_tick
            : 0;
        g_state.elapsed_seconds =
            static_cast<double>(elapsed_ticks) * game_clock::kFixedStepSeconds;
        complete = g_state.elapsed_seconds + 1e-9 >= g_state.required_seconds;
        if (complete) {
            g_state.active = false;
            ++g_state.completion_count;
        }
    }

    // OpenTheDoor(true, ...) stores the interaction byte immediately when the
    // final Carousel callback fires; there is no additional open-door delay on
    // the menuOpenGC gate itself.
    if (complete) {
        single_select_hero_state::complete_transition();
    }
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = {};
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "single select hero transition: " << (state.active ? "running" : "idle")
        << " generation=" << state.selector_generation
        << " selection=" << state.selector_selection_count
        << " from=" << state.from_career
        << " to=" << state.to_career
        << " door-close=" << (state.includes_door_close ? "yes" : "no")
        << " elapsed=" << state.elapsed_seconds
        << " required=" << state.required_seconds
        << " completions=" << state.completion_count
        << "\n";
    return out.str();
}

}  // namespace nevergone::single_select_hero_transition_state
