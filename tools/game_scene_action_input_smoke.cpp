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
    // cannot mutate tutorial progression.
    assert(action::observe(action::Control::kButtonC, action::Gesture::kTap));
    assert(tutorial::snapshot().step == tutorial::Step::kInactive);
    assert(action::snapshot().tutorial_event_count == 0);

    enter_interactive_jump();
    action::reset();

    // Wrong button/gesture must not advance the Jump lesson.
    assert(action::observe(action::Control::kButtonA, action::Gesture::kTap));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);
    assert(action::observe(action::Control::kButtonC, action::Gesture::kPress));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);

    // The observed lesson contract is "Tap C to jump". Once the recovered
    // Controls geometry proves a C tap and calls this semantic boundary, the
    // tutorial advances exactly once to Attack.
    assert(action::observe(action::Control::kButtonC, action::Gesture::kTap));
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
    auto state = action::snapshot();
    assert(state.tutorial_event_count == 1);
    assert(state.accepted_input_count == 3);

    // Attack is objective-gated. Repeated A taps are recorded but do not skip
    // the required enemy-defeat event.
    assert(action::observe(action::Control::kButtonA, action::Gesture::kTap));
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
    assert(action::snapshot().tutorial_event_count == 1);

    const std::string status = action::status_report();
    assert(contains(status, "last control: button-a"));
    assert(contains(status, "last original Controls state: 7"));
    assert(contains(status, "last gesture: tap"));
    assert(contains(status, "tutorial events: 1"));
    return 0;
}
