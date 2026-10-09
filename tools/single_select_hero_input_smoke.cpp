#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "game_clock.h"
#include "single_select_hero_input.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_timeline.h"
#include "startup_contract.h"

int main() {
    namespace fs = std::filesystem;
    namespace input = nevergone::single_select_hero_input;
    namespace selector = nevergone::single_select_hero_state;
    namespace timeline = nevergone::single_select_hero_transition_timeline;

    const fs::path root = fs::temp_directory_path() / "nevergone_single_select_input";
    fs::remove_all(root);
    fs::create_directories(root / "assets");
    std::ofstream(root / "assets" / "newWord.txt") << "blocked\n";
    nevergone::startup::RuntimeConfig config;
    config.files_dir = root.string();
    nevergone::startup::configure(config);

    nevergone::game_clock::reset();
    timeline::reset();
    selector::reset();
    input::reset();

    assert(!input::on_touch_for_surface(0, 0, 568.0f, 145.0f, 1136, 640));

    selector::begin(0);
    assert(!selector::snapshot().input_enabled);
    assert(input::on_touch_for_surface(0, 1, 568.0f, 145.0f, 1136, 640));
    assert(input::pressed_tag() == 0);

    selector::complete_transition();

    // Career 3 center: design y=315 -> top-origin surface y=325.
    assert(input::on_touch_for_surface(0, 7, 568.0f, 325.0f, 1136, 640));
    assert(input::pressed_tag() == 3);
    assert(input::on_touch_for_surface(2, 7, 400.0f, 325.0f, 1136, 640));
    assert(input::pressed_tag() == 0);
    assert(input::on_touch_for_surface(2, 7, 568.0f, 325.0f, 1136, 640));
    assert(input::pressed_tag() == 3);
    assert(input::on_touch_for_surface(1, 7, 568.0f, 325.0f, 1136, 640));

    auto state = selector::snapshot();
    assert(state.selected_career == 3);
    assert(state.transition_pending);
    assert(!state.input_enabled);
    assert(state.selection_count == 1);

    auto transition = timeline::snapshot();
    assert(transition.phase == timeline::Phase::kCareerChange);
    assert(transition.selector_generation == state.generation);
    assert(transition.old_career == 1);
    assert(transition.new_career == 3);
    assert(transition.begin_tick == nevergone::game_clock::tick_count());
    assert(transition.unlock_tick ==
        transition.begin_tick + timeline::kNearCareerChangeUnlockTicks);

    // Locked input is consumed and cannot replace the pending selection.
    assert(input::on_touch_for_surface(0, 8, 568.0f, 505.0f, 1136, 640));
    assert(input::on_touch_for_surface(1, 8, 568.0f, 505.0f, 1136, 640));
    assert(selector::snapshot().selected_career == 3);

    selector::complete_transition();
    timeline::reset();

    // Same-career click is a handled no-op and does not arm a new deadline.
    assert(input::on_touch_for_surface(0, 9, 568.0f, 325.0f, 1136, 640));
    assert(input::on_touch_for_surface(1, 9, 568.0f, 325.0f, 1136, 640));
    state = selector::snapshot();
    assert(state.selected_career == 3);
    assert(state.input_enabled);
    assert(!state.transition_pending);
    assert(timeline::snapshot().phase == timeline::Phase::kInactive);

    // Career 5 is a near change from 3 and arms the same 179-tick deadline.
    assert(input::on_touch_for_surface(0, 10, 568.0f, 505.0f, 1136, 640));
    assert(input::on_touch_for_surface(1, 10, 568.0f, 505.0f, 1136, 640));
    transition = timeline::snapshot();
    assert(transition.old_career == 3);
    assert(transition.new_career == 5);
    assert(transition.unlock_tick - transition.begin_tick ==
        timeline::kNearCareerChangeUnlockTicks);

    // Complete it, then test a far 5 -> 1 change and its 231-tick deadline.
    selector::complete_transition();
    timeline::reset();
    assert(input::on_touch_for_surface(0, 11, 568.0f, 145.0f, 1136, 640));
    assert(input::on_touch_for_surface(1, 11, 568.0f, 145.0f, 1136, 640));
    transition = timeline::snapshot();
    assert(transition.old_career == 5);
    assert(transition.new_career == 1);
    assert(transition.unlock_tick - transition.begin_tick ==
        timeline::kFarCareerChangeUnlockTicks);

    input::reset();
    fs::remove_all(root);
    std::cout << "single select hero input smoke: ok\n";
    return 0;
}
