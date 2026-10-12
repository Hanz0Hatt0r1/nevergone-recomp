#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "game_clock.h"
#include "single_select_hero_input.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_timeline.h"
#include "startup_contract.h"

int main() {
    namespace fs = std::filesystem;
    namespace input = nevergone::single_select_hero_input;
    namespace rune_layout = nevergone::single_select_hero_rune_layout;
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
    assert(rune_layout::pressed_render_tag() == 0);

    assert(!input::on_touch_for_surface(0, 0, 568.0f, 145.0f, 1136, 640));

    selector::begin(0);
    assert(!selector::snapshot().input_enabled);
    assert(input::on_touch_for_surface(0, 1, 568.0f, 145.0f, 1136, 640));
    assert(input::pressed_tag() == 0);
    assert(rune_layout::pressed_render_tag() == 0);

    selector::complete_transition();

    // The paired frozen captures expose only career tags 1 and 2 as active
    // menu items. The selector surface consumes an outside/hidden DOWN while
    // active, but it must not capture a rune gesture or change the selection.
    assert(input::on_touch_for_surface(0, 6, 568.0f, 325.0f, 1136, 640));
    assert(input::pressed_tag() == 0);
    assert(rune_layout::pressed_render_tag() == 0);
    assert(selector::snapshot().selected_career == 1);

    // Career 2 center: design y=405 -> top-origin surface y=235.
    assert(input::on_touch_for_surface(0, 7, 568.0f, 235.0f, 1136, 640));
    assert(input::pressed_tag() == 2);
    assert(rune_layout::pressed_render_tag() == 2);
    assert(rune_layout::frame_index(2, false) == 3);
    assert(input::on_touch_for_surface(2, 7, 400.0f, 235.0f, 1136, 640));
    assert(input::pressed_tag() == 0);
    assert(rune_layout::pressed_render_tag() == 0);
    assert(input::on_touch_for_surface(2, 7, 568.0f, 235.0f, 1136, 640));
    assert(input::pressed_tag() == 2);
    assert(rune_layout::pressed_render_tag() == 2);
    assert(input::on_touch_for_surface(1, 7, 568.0f, 235.0f, 1136, 640));
    assert(input::pressed_tag() == 0);
    assert(rune_layout::pressed_render_tag() == 0);

    auto state = selector::snapshot();
    assert(state.selected_career == 2);
    assert(state.transition_pending);
    assert(!state.input_enabled);
    assert(state.selection_count == 1);

    auto transition = timeline::snapshot();
    assert(transition.phase == timeline::Phase::kCareerChange);
    assert(transition.selector_generation == state.generation);
    assert(transition.old_career == 1);
    assert(transition.new_career == 2);
    assert(transition.begin_tick == nevergone::game_clock::tick_count());
    assert(transition.unlock_tick ==
        transition.begin_tick + timeline::kNearCareerChangeUnlockTicks);

    // Locked input is consumed and cannot replace the pending selection or
    // publish a pressed presentation state.
    assert(input::on_touch_for_surface(0, 8, 568.0f, 145.0f, 1136, 640));
    assert(input::on_touch_for_surface(1, 8, 568.0f, 145.0f, 1136, 640));
    assert(selector::snapshot().selected_career == 2);
    assert(rune_layout::pressed_render_tag() == 0);

    selector::complete_transition();
    timeline::reset();

    // Same-career click is a handled no-op and does not arm a new deadline.
    assert(input::on_touch_for_surface(0, 9, 568.0f, 235.0f, 1136, 640));
    assert(rune_layout::pressed_render_tag() == 2);
    assert(input::on_touch_for_surface(1, 9, 568.0f, 235.0f, 1136, 640));
    assert(rune_layout::pressed_render_tag() == 0);
    state = selector::snapshot();
    assert(state.selected_career == 2);
    assert(state.input_enabled);
    assert(!state.transition_pending);
    assert(timeline::snapshot().phase == timeline::Phase::kInactive);

    // The other visible rune remains interactive and arms the recovered near
    // transition back from career 2 to career 1.
    assert(input::on_touch_for_surface(0, 10, 568.0f, 145.0f, 1136, 640));
    assert(rune_layout::pressed_render_tag() == 1);
    assert(input::on_touch_for_surface(1, 10, 568.0f, 145.0f, 1136, 640));
    assert(rune_layout::pressed_render_tag() == 0);
    transition = timeline::snapshot();
    assert(transition.old_career == 2);
    assert(transition.new_career == 1);
    assert(transition.unlock_tick - transition.begin_tick ==
        timeline::kNearCareerChangeUnlockTicks);

    input::reset();
    assert(rune_layout::pressed_render_tag() == 0);
    fs::remove_all(root);
    std::cout << "single select hero input smoke: ok\n";
    return 0;
}
