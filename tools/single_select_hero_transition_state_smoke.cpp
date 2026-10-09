#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "character_name_state.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_state.h"
#include "startup_contract.h"

namespace {
bool close_enough(double lhs, double rhs) {
    return std::fabs(lhs - rhs) < 1e-9;
}
}

int main() {
    namespace fs = std::filesystem;
    namespace selector = nevergone::single_select_hero_state;
    namespace transition = nevergone::single_select_hero_transition_state;

    const fs::path root = fs::temp_directory_path() / "nevergone_single_select_transition";
    fs::remove_all(root);
    fs::create_directories(root / "assets");
    std::ofstream(root / "assets" / "newWord.txt") << "blocked\n";
    nevergone::startup::RuntimeConfig config;
    config.files_dir = root.string();
    nevergone::startup::configure(config);

    assert(close_enough(transition::transition_seconds(1, 1, false), 2.4));
    assert(close_enough(transition::transition_seconds(1, 2, true), 4.4));
    assert(close_enough(transition::transition_seconds(1, 3, true), 4.4));
    assert(close_enough(transition::transition_seconds(1, 4, true), 5.9));
    assert(close_enough(transition::transition_seconds(5, 2, true), 5.9));
    assert(close_enough(transition::transition_seconds(5, 3, true), 4.4));

    transition::reset();
    selector::reset();
    selector::begin(0);
    auto state = selector::snapshot();
    assert(state.transition_from_career == 1);
    assert(state.selected_career == 1);

    // Initial initUI: Carousel(selected) starts immediately. At 35 Hz, 2.4s
    // is exactly 84 fixed ticks. The gate stays closed through tick 83.
    transition::sync(1000);
    auto timeline = transition::snapshot();
    assert(timeline.active);
    assert(!timeline.includes_door_close);
    assert(close_enough(timeline.required_seconds, 2.4));
    transition::sync(1083);
    assert(!selector::snapshot().input_enabled);
    transition::sync(1084);
    assert(selector::snapshot().input_enabled);
    assert(!selector::snapshot().transition_pending);
    assert(transition::snapshot().completion_count == 1);

    // Near changed transition: menuOpenGC old=1 -> new=3, then 2.0s door close
    // + (1.5 + 0.3 + 0.6)s Carousel = 4.4s = 154 fixed ticks.
    assert(selector::select_career(3));
    state = selector::snapshot();
    assert(state.transition_from_career == 1);
    assert(state.selected_career == 3);
    transition::sync(2000);
    timeline = transition::snapshot();
    assert(timeline.includes_door_close);
    assert(close_enough(timeline.required_seconds, 4.4));
    transition::sync(2153);
    assert(!selector::snapshot().input_enabled);
    transition::sync(2154);
    assert(selector::snapshot().input_enabled);

    // Far transition: old=3 -> new=Five has distance 2, still the 1.5s branch.
    assert(selector::select_career(5));
    transition::sync(3000);
    assert(close_enough(transition::snapshot().required_seconds, 4.4));
    transition::sync(3154);
    assert(selector::snapshot().input_enabled);

    // Distance > 2 takes the shipped 3.0s first Carousel leg. Total 5.9s;
    // on a 35 Hz fixed clock the first tick at/after it is tick 207.
    assert(selector::select_career(1));
    state = selector::snapshot();
    assert(state.transition_from_career == 5);
    transition::sync(4000);
    assert(close_enough(transition::snapshot().required_seconds, 5.9));
    transition::sync(4206);
    assert(!selector::snapshot().input_enabled);
    transition::sync(4207);
    assert(selector::snapshot().input_enabled);
    assert(transition::snapshot().completion_count == 4);

    selector::reset();
    transition::sync(5000);
    assert(!transition::snapshot().active);

    fs::remove_all(root);
    std::cout << "single select hero transition state smoke: ok\n";
    return 0;
}
