#include <cassert>
#include <string>

#include "game_scene_action_input.h"
#include "tutorial_gameplay_state.h"
#include "tutorial_session_state.h"

namespace action = nevergone::game_scene_action_input;
namespace tutorial = nevergone::tutorial_gameplay_state;
namespace session = nevergone::tutorial_session_state;

namespace {

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

void enter_interactive_jump() {
    session::reset();
    tutorial::reset();
    session::begin_loading();
    assert(session::on_loading_complete());
    assert(session::on_lesson_overlay_complete());
    assert(session::on_dialogue_complete());
    assert(tutorial::snapshot().step == tutorial::Step::kRun);
    assert(tutorial::observe(tutorial::Event::kRunPerformed));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);
}

}  // namespace

int main() {
    action::reset();
    session::reset();
    tutorial::reset();

    assert(!action::original_controls_state(action::Control::kNone).has_value());
    assert(action::original_controls_state(action::Control::kButtonA) == 7);
    assert(action::original_controls_state(action::Control::kButtonB) == 8);
    assert(action::original_controls_state(action::Control::kButtonC) == 9);
    assert(action::control_for_original_controls_state(7) == action::Control::kButtonA);
    assert(action::control_for_original_controls_state(8) == action::Control::kButtonB);
    assert(action::control_for_original_controls_state(9) == action::Control::kButtonC);
    assert(!action::control_for_original_controls_state(6).has_value());
    assert(!action::control_for_original_controls_state(10).has_value());

    assert(!action::observe(action::Control::kNone, action::Gesture::kTap));
    assert(action::snapshot().rejected_input_count == 1);

    // A semantic C tap outside interactive tutorial is accepted as input but
    // cannot request or complete the tutorial jump.
    assert(action::observe(action::Control::kButtonC, action::Gesture::kTap));
    assert(tutorial::snapshot().step == tutorial::Step::kInactive);
    assert(action::snapshot().tutorial_jump_request_count == 0);
    assert(action::snapshot().tutorial_event_count == 0);
    assert(!action::observe_live_meplayer_state(0x20u));

    enter_interactive_jump();
    action::reset();

    // Wrong button/gesture must neither request state 0x20 nor advance Jump.
    assert(action::observe(action::Control::kButtonA, action::Gesture::kTap));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);
    assert(action::observe(action::Control::kButtonC, action::Gesture::kPress));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);
    assert(action::snapshot().tutorial_jump_request_count == 0);

    // The C tap is only the request that starts the live action load. It must
    // not skip the recovered LessonLayer MEPLAYERSTATE == 0x20 gate.
    assert(action::observe(action::Control::kButtonC, action::Gesture::kTap));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);
    auto state = action::snapshot();
    assert(state.tutorial_jump_request_count == 1);
    assert(state.tutorial_event_count == 0);
    assert(state.accepted_input_count == 3);

    // A different committed player state cannot satisfy the Jump lesson.
    assert(!action::observe_live_meplayer_state(0x1bu));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);

    // Only the recovered live jump state advances to Attack.
    assert(action::observe_live_meplayer_state(0x20u));
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
    state = action::snapshot();
    assert(state.tutorial_jump_request_count == 1);
    assert(state.tutorial_event_count == 1);

    // Re-observing 0x20 is idempotent once Attack owns the tutorial.
    assert(!action::observe_live_meplayer_state(0x20u));
    assert(action::snapshot().tutorial_event_count == 1);

    // Attack is objective-gated. Repeated A taps are recorded but do not skip
    // the required enemy-defeat event.
    assert(action::observe(action::Control::kButtonA, action::Gesture::kTap));
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
    assert(action::snapshot().tutorial_event_count == 1);

    const std::string status = action::status_report();
    assert(contains(status, "last control: button-a"));
    assert(contains(status, "last original Controls state: 7"));
    assert(contains(status, "last gesture: tap"));
    assert(contains(status, "tutorial jump requests: 1"));
    assert(contains(status, "tutorial events: 1"));
    return 0;
}
