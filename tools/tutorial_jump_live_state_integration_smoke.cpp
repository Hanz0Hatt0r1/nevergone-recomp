#include <cassert>

#include "game_scene_action_input.h"
#include "meplayer_live_action_state.h"
#include "tutorial_gameplay_state.h"
#include "tutorial_session_state.h"

namespace action = nevergone::game_scene_action_input;
namespace live = nevergone::meplayer_live_action_state;
namespace tutorial = nevergone::tutorial_gameplay_state;
namespace session = nevergone::tutorial_session_state;

namespace {

void enter_interactive_jump() {
    action::reset();
    live::reset();
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
    enter_interactive_jump();

    // A fresh player starts on recovered action28 / MEPLAYERSTATE 0x1b.
    assert(live::sync_spawn(41u, 0u));
    assert(live::commit_loaded_action(live::kInitialActionState));
    assert(live::snapshot().current_action_state == live::kInitialActionState);

    // The semantic C tap is retained even if DOWN/UP both happen before the
    // next render/staging pass, but it must not advance LessonLayer yet.
    assert(action::observe(action::Control::kButtonC, action::Gesture::kTap));
    const auto input = action::snapshot();
    assert(input.tutorial_jump_request_count == 1u);
    assert(input.tutorial_event_count == 0u);
    assert(tutorial::snapshot().step == tutorial::Step::kJump);

    // PlayerActionRuntimeLoader's native desired-state path consumes that
    // monotonic request and queues the recovered setActionState(0x20,false).
    assert(live::observe_tutorial_jump_request_count(
        input.tutorial_jump_request_count));
    assert(live::snapshot().desired_action_state == live::kTutorialJumpActionState);
    assert(tutorial::snapshot().step == tutorial::Step::kJump);

    // Loading/committing action33 makes 0x20 the concrete live state. Only now
    // may LessonLayer's recovered MEPLAYERSTATE gate publish Jump completion.
    assert(live::commit_loaded_action(live::kTutorialJumpActionState));
    assert(live::snapshot().current_action_state == live::kTutorialJumpActionState);
    assert(action::observe_live_meplayer_state(live::kTutorialJumpActionState));
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
    assert(action::snapshot().tutorial_event_count == 1u);

    // Repeated commits/observations cannot advance another tutorial step.
    assert(live::commit_loaded_action(live::kTutorialJumpActionState));
    assert(!action::observe_live_meplayer_state(live::kTutorialJumpActionState));
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
    assert(action::snapshot().tutorial_event_count == 1u);

    return 0;
}
