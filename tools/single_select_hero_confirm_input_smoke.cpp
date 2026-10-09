#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "character_name_state.h"
#include "single_select_hero_confirm_input.h"
#include "single_select_hero_confirm_layout.h"
#include "single_select_hero_state.h"
#include "startup_contract.h"

namespace {
bool close_enough(float lhs, float rhs) {
    return lhs > rhs - 0.0001f && lhs < rhs + 0.0001f;
}
}

int main() {
    namespace fs = std::filesystem;
    namespace confirm = nevergone::single_select_hero_confirm_input;
    namespace layout = nevergone::single_select_hero_confirm_layout;
    namespace character = nevergone::character_name_state;
    namespace selector = nevergone::single_select_hero_state;

    const auto rect = layout::hit_rect_for_surface(1136, 640);
    assert(rect.valid);
    assert(close_enough(rect.left, 751.0f));
    assert(close_enough(rect.top, 532.5f));
    assert(close_enough(rect.right, 921.0f));
    assert(close_enough(rect.bottom, 607.5f));
    assert(layout::hit_test(1136, 640, 836.0f, 570.0f));
    assert(!layout::hit_test(1136, 640, 750.9f, 570.0f));

    const auto rect2x = layout::hit_rect_for_surface(2272, 1280);
    assert(rect2x.valid);
    assert(close_enough(rect2x.left, 1502.0f));
    assert(close_enough(rect2x.top, 1065.0f));
    assert(close_enough(rect2x.right, 1842.0f));
    assert(close_enough(rect2x.bottom, 1215.0f));

    // Exact recovered atlas geometry for the parent and normal/pressed menu
    // frames. btn_d has a two-pixel horizontal trim inside its 170x75 source.
    layout::FrameGeometry parent;
    parent.width = 177;
    parent.height = 80;
    parent.source_width = 177;
    parent.source_height = 80;
    auto quad = layout::quad_for_surface(parent, 1136, 640);
    assert(quad.valid);
    assert(close_enough(quad.x0, 747.5f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.x1, 924.5f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.y0, 1.0f - 530.0f * 2.0f / 640.0f));
    assert(close_enough(quad.y1, 1.0f - 610.0f * 2.0f / 640.0f));

    layout::FrameGeometry normal;
    normal.width = 166;
    normal.height = 75;
    normal.left = 2;
    normal.source_width = 170;
    normal.source_height = 75;
    quad = layout::quad_for_surface(normal, 1136, 640);
    assert(quad.valid);
    assert(close_enough(quad.x0, 753.0f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.x1, 919.0f * 2.0f / 1136.0f - 1.0f));

    layout::FrameGeometry pressed;
    pressed.width = 170;
    pressed.height = 75;
    pressed.source_width = 170;
    pressed.source_height = 75;
    quad = layout::quad_for_surface(pressed, 1136, 640);
    assert(quad.valid);
    assert(close_enough(quad.x0, 751.0f * 2.0f / 1136.0f - 1.0f));
    assert(close_enough(quad.x1, 921.0f * 2.0f / 1136.0f - 1.0f));

    layout::FrameGeometry invalid = normal;
    invalid.width = 171;
    assert(!layout::quad_for_surface(invalid, 1136, 640).valid);

    const fs::path root = fs::temp_directory_path() / "nevergone_single_select_confirm";
    fs::remove_all(root);
    fs::create_directories(root / "assets");
    std::ofstream(root / "assets" / "newWord.txt") << "blocked\n";
    nevergone::startup::RuntimeConfig config;
    config.files_dir = root.string();
    nevergone::startup::configure(config);

    selector::reset();
    character::reset();
    confirm::reset();

    assert(!confirm::on_touch_for_surface(0, 1, 836.0f, 570.0f, 1136, 640));
    assert(!confirm::pressed());

    selector::begin(0);
    // menuConfirm itself does not test the OpenTheDoor/menuOpenGC interaction
    // gate, so it remains actionable during the selector's initial transition.
    assert(!selector::snapshot().input_enabled);
    assert(!confirm::on_touch_for_surface(0, 2, 500.0f, 570.0f, 1136, 640));
    assert(confirm::on_touch_for_surface(0, 2, 836.0f, 570.0f, 1136, 640));
    assert(confirm::pressed());

    // Once Confirm owns a pointer, secondary pointers remain consumed so they
    // cannot leak into the rune router underneath the same gesture.
    assert(confirm::on_touch_for_surface(5, 9, 568.0f, 325.0f, 1136, 640));
    assert(confirm::pressed());
    assert(confirm::on_touch_for_surface(6, 9, 568.0f, 325.0f, 1136, 640));
    assert(confirm::pressed());

    assert(confirm::on_touch_for_surface(2, 2, 700.0f, 570.0f, 1136, 640));
    assert(!confirm::pressed());
    assert(confirm::on_touch_for_surface(2, 2, 836.0f, 570.0f, 1136, 640));
    assert(confirm::pressed());
    assert(confirm::on_touch_for_surface(1, 2, 836.0f, 570.0f, 1136, 640));
    assert(!confirm::pressed());

    auto selector_state = selector::snapshot();
    auto character_state = character::snapshot();
    assert(selector_state.confirm_count == 1);
    assert(selector_state.character_name_open_count == 1);
    assert(character_state.active);
    assert(character_state.career == 1);

    // Cancellation clears presentation state and does not confirm.
    character::reset();
    assert(confirm::on_touch_for_surface(0, 3, 836.0f, 570.0f, 1136, 640));
    assert(confirm::pressed());
    assert(confirm::on_touch_for_surface(3, 3, 836.0f, 570.0f, 1136, 640));
    assert(!confirm::pressed());
    assert(selector::snapshot().confirm_count == 1);

    // Existing-career equality remains owned by confirm_online().
    selector::reset();
    character::reset();
    confirm::reset();
    selector::begin(1);  // starts on career 2
    selector::complete_transition();
    assert(selector::select_career(1));
    selector::complete_transition();
    assert(confirm::on_touch_for_surface(0, 4, 836.0f, 570.0f, 1136, 640));
    assert(confirm::on_touch_for_surface(1, 4, 836.0f, 570.0f, 1136, 640));
    selector_state = selector::snapshot();
    assert(selector_state.confirm_count == 1);
    assert(selector_state.blocked_confirm_count == 1);
    assert(selector_state.character_name_open_count == 0);
    assert(!character::snapshot().active);

    confirm::reset();
    fs::remove_all(root);
    std::cout << "single select hero confirm input smoke: ok\n";
    return 0;
}
