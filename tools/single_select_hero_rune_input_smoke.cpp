#include <cassert>

#include "game_clock.h"
#include "offline_startup_flow.h"
#include "single_select_hero_rune_input.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_timeline.h"

namespace {

void publish_normal_frame(int tag, int source_width, int source_height) {
    nevergone::single_select_hero_rune_layout::FrameGeometry frame;
    frame.width = source_width;
    frame.height = source_height;
    frame.source_width = source_width;
    frame.source_height = source_height;
    const auto quad = nevergone::single_select_hero_rune_layout::quad_for_surface(
        frame, tag, 1136, 640);
    assert(quad.valid);
}

float touch_y_for_tag(int tag) {
    return nevergone::single_select_hero_rune_layout::kDesignHeight -
        nevergone::single_select_hero_rune_layout::center_y(tag);
}

}  // namespace

int main() {
    namespace input = nevergone::single_select_hero_rune_input;
    namespace layout = nevergone::single_select_hero_rune_layout;
    namespace selector = nevergone::single_select_hero_state;
    namespace timeline = nevergone::single_select_hero_transition_timeline;

    nevergone::game_clock::reset();
    nevergone::offline_startup_flow::reset();
    selector::reset();
    timeline::reset();
    input::reset();

    nevergone::offline_startup_flow::set_standalone_hero_presence(false);
    nevergone::offline_startup_flow::on_auto_login_compat_success(1);
    assert(nevergone::offline_startup_flow::snapshot().route ==
        nevergone::offline_startup_flow::Route::kOpeningDialogue);

    selector::begin(0);
    selector::complete_transition();
    auto state = selector::snapshot();
    assert(state.selected_career == 1);
    assert(state.input_enabled);

    publish_normal_frame(1, 79, 99);
    publish_normal_frame(2, 87, 89);
    publish_normal_frame(3, 75, 95);
    publish_normal_frame(4, 111, 110);
    publish_normal_frame(5, 137, 131);
    assert(layout::runtime_surface_width() == 1136);
    assert(layout::runtime_surface_height() == 640);
    assert(layout::runtime_source_width(5) == 137);
    assert(layout::runtime_source_height(5) == 131);

    const float x = layout::kCenterX;
    const float career_two_y = touch_y_for_tag(2);

    assert(input::on_touch(0, 7, x, career_two_y));
    assert(input::pressed_tag() == 2);
    assert(layout::runtime_pressed_tag() == 2);
    assert(layout::frame_index(2, false) == 3);
    assert(selector::snapshot().selected_career == 1);

    assert(input::on_touch(2, 7, 10.0f, 10.0f));
    assert(input::pressed_tag() == 0);
    assert(layout::runtime_pressed_tag() == 0);
    assert(layout::frame_index(2, false) == 2);

    assert(input::on_touch(2, 7, x, career_two_y));
    assert(input::pressed_tag() == 2);
    assert(input::on_touch(1, 7, x, career_two_y));
    assert(input::pressed_tag() == 0);
    state = selector::snapshot();
    assert(state.selected_career == 2);
    assert(state.selection_count == 1);
    assert(!state.input_enabled);
    assert(state.transition_pending);

    auto transition = timeline::snapshot();
    assert(transition.phase == timeline::Phase::kCareerChange);
    assert(transition.old_career == 1);
    assert(transition.new_career == 2);
    assert(transition.unlock_tick == nevergone::game_clock::tick_count() +
        timeline::kNearCareerChangeUnlockTicks);

    // The menu item can still show a press while the recovered handler gate is
    // locked; release is consumed but select_career rejects the mutation.
    const float career_three_y = touch_y_for_tag(3);
    assert(input::on_touch(0, 8, x, career_three_y));
    assert(input::pressed_tag() == 3);
    assert(input::on_touch(1, 8, x, career_three_y));
    assert(selector::snapshot().selected_career == 2);

    // After unlock, a far 2 -> 5 change must arm the longer 231-tick timeline.
    selector::complete_transition();
    timeline::reset();
    const float career_five_y = touch_y_for_tag(5);
    assert(input::on_touch(0, 9, x, career_five_y));
    assert(input::on_touch(1, 9, x, career_five_y));
    state = selector::snapshot();
    assert(state.selected_career == 5);
    transition = timeline::snapshot();
    assert(transition.old_career == 2);
    assert(transition.new_career == 5);
    assert(transition.unlock_tick == nevergone::game_clock::tick_count() +
        timeline::kFarCareerChangeUnlockTicks);

    selector::complete_transition();
    timeline::reset();
    assert(input::on_touch(0, 10, x, career_five_y));
    assert(input::on_touch(1, 10, x, career_five_y));
    assert(selector::snapshot().selection_count == 2);
    assert(timeline::snapshot().phase == timeline::Phase::kInactive);

    assert(input::on_touch(0, 11, x, touch_y_for_tag(4)));
    assert(input::pressed_tag() == 4);
    assert(input::on_touch(3, 11, x, touch_y_for_tag(4)));
    assert(input::pressed_tag() == 0);
    assert(selector::snapshot().selected_career == 5);

    nevergone::offline_startup_flow::reset();
    assert(!input::on_touch(0, 12, x, career_two_y));
    assert(input::pressed_tag() == 0);
    return 0;
}
