#include <cassert>
#include <cstdint>
#include <iostream>

#include "character_name_state.h"
#include "game_clock.h"
#include "offline_startup_flow.h"
#include "single_select_hero_pointer_router.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_timeline.h"

namespace {

void begin_opening_dialogue() {
    nevergone::offline_startup_flow::reset();
    nevergone::offline_startup_flow::set_standalone_hero_presence(false);
    nevergone::offline_startup_flow::on_auto_login_compat_success(17);
    assert(nevergone::offline_startup_flow::snapshot().route ==
           nevergone::offline_startup_flow::Route::kOpeningDialogue);
}

void reset_selector(bool unlock) {
    nevergone::character_name_state::reset();
    nevergone::single_select_hero_transition_timeline::reset();
    nevergone::single_select_hero_state::reset();
    nevergone::single_select_hero_state::begin(0);
    if (unlock) nevergone::single_select_hero_state::complete_transition();
    nevergone::single_select_hero_pointer_router::reset();
    nevergone::single_select_hero_pointer_router::set_surface_size(1136, 640);
}

}  // namespace

int main() {
    using nevergone::single_select_hero_transition_timeline::Phase;

    nevergone::game_clock::reset();
    begin_opening_dialogue();
    reset_selector(true);

    auto selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.input_enabled);
    assert(selector.selected_career == 1);

    // Career 3 center in Android top-origin surface coordinates. Recovered
    // Cocos center is (568,315), therefore surface Y is 640-315 = 325.
    assert(nevergone::single_select_hero_pointer_router::on_touch(
        0, 3, 568.0f, 325.0f));
    assert(nevergone::single_select_hero_pointer_router::on_touch(
        2, 3, 568.0f, 325.0f));
    assert(nevergone::single_select_hero_pointer_router::on_touch(
        1, 3, 568.0f, 325.0f));

    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.selected_career == 3);
    assert(!selector.input_enabled);
    assert(selector.transition_pending);

    auto timeline = nevergone::single_select_hero_transition_timeline::snapshot();
    assert(timeline.phase == Phase::kCareerChange);
    assert(timeline.selector_generation == selector.generation);
    assert(timeline.begin_tick == nevergone::game_clock::tick_count());
    assert(timeline.unlock_tick == timeline.begin_tick +
           nevergone::single_select_hero_transition_timeline::kCareerChangeUnlockTicks);

    // Input is locked immediately after the career change; a new DOWN must not
    // capture until the recovered transition callback unlocks it.
    assert(!nevergone::single_select_hero_pointer_router::on_touch(
        0, 4, 568.0f, 415.0f));

    // Releasing outside the originally armed rune cancels activation.
    reset_selector(true);
    assert(nevergone::single_select_hero_pointer_router::on_touch(
        0, 1, 568.0f, 415.0f));  // career 2 center
    assert(nevergone::single_select_hero_pointer_router::on_touch(
        1, 1, 10.0f, 10.0f));
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.selected_career == 1);
    assert(selector.input_enabled);
    assert(nevergone::single_select_hero_transition_timeline::snapshot().phase ==
           Phase::kInactive);

    // Same-career release is owned but does not start another transition.
    assert(nevergone::single_select_hero_pointer_router::on_touch(
        0, 2, 568.0f, 145.0f));  // career 1 center
    assert(nevergone::single_select_hero_pointer_router::on_touch(
        1, 2, 568.0f, 145.0f));
    assert(nevergone::single_select_hero_state::snapshot().selected_career == 1);
    assert(nevergone::single_select_hero_transition_timeline::snapshot().phase ==
           Phase::kInactive);

    // Initial transition lock is respected: unlike the temporary compatibility
    // buttons, the native rune path does not collapse it.
    reset_selector(false);
    assert(!nevergone::single_select_hero_pointer_router::on_touch(
        0, 5, 568.0f, 505.0f));

    // CharacterNameLayer is modal over the selector; runes underneath it must
    // not become interactive even if the selector state itself remains alive.
    reset_selector(true);
    nevergone::character_name_state::begin(2);
    assert(!nevergone::single_select_hero_pointer_router::on_touch(
        0, 6, 568.0f, 415.0f));

    std::cout << "single-select-hero pointer router smoke: ok\n";
    return 0;
}
