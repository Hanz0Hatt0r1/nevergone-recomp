#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "single_select_hero_input.h"
#include "single_select_hero_state.h"
#include "startup_contract.h"

int main() {
    namespace fs = std::filesystem;
    namespace input = nevergone::single_select_hero_input;
    namespace selector = nevergone::single_select_hero_state;

    const fs::path root = fs::temp_directory_path() / "nevergone_single_select_input";
    fs::remove_all(root);
    fs::create_directories(root / "assets");
    std::ofstream(root / "assets" / "newWord.txt") << "blocked\n";
    nevergone::startup::RuntimeConfig config;
    config.files_dir = root.string();
    nevergone::startup::configure(config);

    selector::reset();
    input::reset();
    assert(!input::on_touch_for_surface(0, 0, 568.0f, 145.0f, 1136, 640));

    selector::begin(0);
    auto state = selector::snapshot();
    assert(!state.input_enabled);
    assert(input::on_touch_for_surface(0, 0, 568.0f, 145.0f, 1136, 640));
    assert(input::pressed_tag() == 0);
    assert(selector::snapshot().selected_career == 1);

    selector::complete_transition();
    state = selector::snapshot();
    assert(state.input_enabled);

    // Career 3 center is design (568,315), i.e. top-origin surface y=325.
    assert(input::on_touch_for_surface(0, 7, 568.0f, 325.0f, 1136, 640));
    assert(input::pressed_tag() == 3);
    assert(input::on_touch_for_surface(2, 7, 400.0f, 325.0f, 1136, 640));
    assert(input::pressed_tag() == 0);
    assert(input::on_touch_for_surface(2, 7, 568.0f, 325.0f, 1136, 640));
    assert(input::pressed_tag() == 3);
    assert(input::on_touch_for_surface(1, 7, 568.0f, 325.0f, 1136, 640));
    assert(input::pressed_tag() == 0);

    state = selector::snapshot();
    assert(state.selected_career == 3);
    assert(state.transition_from_career == 1);
    assert(state.transition_pending);
    assert(!state.input_enabled);
    assert(state.selection_count == 1);

    // Locked transition owns input but cannot select another career.
    assert(input::on_touch_for_surface(0, 8, 568.0f, 505.0f, 1136, 640));
    assert(input::on_touch_for_surface(1, 8, 568.0f, 505.0f, 1136, 640));
    assert(selector::snapshot().selected_career == 3);

    selector::complete_transition();

    // Same-career click is handled without starting another transition.
    assert(input::on_touch_for_surface(0, 9, 568.0f, 325.0f, 1136, 640));
    assert(input::on_touch_for_surface(1, 9, 568.0f, 325.0f, 1136, 640));
    state = selector::snapshot();
    assert(state.selected_career == 3);
    assert(state.input_enabled);
    assert(!state.transition_pending);
    assert(state.selection_count == 1);

    // Career 5 center is design y=135 -> top-origin y=505.
    assert(input::on_touch_for_surface(0, 10, 568.0f, 505.0f, 1136, 640));
    assert(input::pressed_tag() == 5);
    assert(input::on_touch_for_surface(3, 10, 568.0f, 505.0f, 1136, 640));
    assert(input::pressed_tag() == 0);
    assert(selector::snapshot().selected_career == 3);

    assert(input::on_touch_for_surface(0, 11, 568.0f, 505.0f, 1136, 640));
    assert(input::on_touch_for_surface(1, 11, 568.0f, 505.0f, 1136, 640));
    state = selector::snapshot();
    assert(state.selected_career == 5);
    assert(state.transition_from_career == 3);
    assert(!state.input_enabled);
    assert(state.selection_count == 2);

    // Aspect-fit mapping remains stable on a 2x surface.
    selector::complete_transition();
    assert(input::on_touch_for_surface(0, 12, 1136.0f, 290.0f, 2272, 1280));
    assert(input::pressed_tag() == 1);
    input::reset();

    fs::remove_all(root);
    std::cout << "single select hero input smoke: ok\n";
    return 0;
}
