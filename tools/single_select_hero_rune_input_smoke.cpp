#include <cassert>

#include "game_clock.h"
#include "offline_startup_flow.h"
#include "single_select_hero_rune_input.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_timeline.h"

namespace {

void publish_surface() {
    nevergone::single_select_hero_rune_layout::FrameGeometry frame;
    frame.width = 79;
    frame.height = 99;
    frame.source_width = 79;
    frame.source_height = 99;
    const auto quad = nevergone::single_select_hero_rune_layout::quad_for_surface(
        frame, 1, 1136, 640);
    assert(quad.valid);
}

float touch_y_for_tag(int tag) {
    return nevergone::single_select_hero_rune_layout::kDesignHeight -
        nevergone::single_select_hero_rune_layout::center_y(tag);
}

void tap(int pointer, int tag) {
    namespace input = nevergone::single_select_hero_rune_input;
    namespace layout = nevergone::single_select_hero_rune_layout;
    const float x = layout::kCenterX;
    const float y = touch_y_for_tag(tag);
    assert(input::on_touch(0, pointer, x, y));
    assert(input::pressed_tag() == tag);
    assert(input::on_touch(1, pointer, x, y));
    assert(input::pressed_tag() == 0);
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
    assert(state.active);
    assert(state.selected_career == 1);
    assert(state.input_enabled);

    publish_surface();
    assert(layout::runtime_surface_width() == 1136);
    assert(layout::runtime_surface_height() == 640);
    assert(layout::recovered_source_width(1) == 79);
    assert(layout::recovered_source_height(2) == 89);
    assert(layout::recovered_source_width(5) == 137);

    const float x = layout::kCenterX;
    const float career_two_y = touch_y_for_tag(2);

    // DOWN switches the existing draw path to xrfuwenfaguang02, without
    // changing semantic selection until a completed release.
    assert(input::on_touch(0, 7, x, career_two_y));
    assert(input::pressed_tag() == 2);
    assert(layout::frame_index(2, false) == 3);
    assert(selector::snapshot().selected_career == 1);

    // Pointer capture survives a move outside while transient glow clears.
    assert(input::on_touch(2, 7, 10.0f, 10.0f));
    assert(input::pressed_tag() == 0);
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

    // While OpenTheDoor owns the interaction gate, visual capture is still a
    // CCMenuItemSprite press, but menuOpenGC semantics reject the mutation.
    tap(8, 3);
    state = selector::snapshot();
    assert(state.selected_career == 2);
    assert(state.selection_count == 1);

    // Same-career selection is a handled no-op and must not arm a transition.
    selector::complete_transition();
    timeline::reset();
    tap(9, 2);
    assert(selector::snapshot().selection_count == 1);
    assert(timeline::snapshot().phase == timeline::Phase::kInactive);

    // A far 2 -> 5 transition must use PR #170's longer Carousel deadline.
    tap(10, 5);
    state = selector::snapshot();
    assert(state.selected_career == 5);
    assert(state.selection_count == 2);
    transition = timeline::snapshot();
    assert(transition.old_career == 2);
    assert(transition.new_career == 5);
    assert(transition.unlock_tick == nevergone::game_clock::tick_count() +
        timeline::kFarCareerChangeUnlockTicks);

    // CANCEL clears only transient pointer ownership/presentation.
    selector::complete_transition();
    timeline::reset();
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
