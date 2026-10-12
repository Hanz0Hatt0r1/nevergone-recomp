#include <cassert>
#include <cstring>
#include <iostream>

#include "tutorial_gameplay_state.h"

int main() {
    namespace tutorial = nevergone::tutorial_gameplay_state;

    tutorial::reset();
    auto state = tutorial::snapshot();
    assert(state.step == tutorial::Step::kInactive);
    assert(!state.active);
    assert(!state.completed);
    assert(!tutorial::observe(tutorial::Event::kRunPerformed));

    tutorial::begin_dialogue_sequence();
    state = tutorial::snapshot();
    assert(state.step == tutorial::Step::kDialogue);
    assert(state.active);
    assert(state.generation == 1);

    // Gameplay cannot start from arbitrary input while the post-LessonLayer
    // conversation still owns the surface.
    assert(!tutorial::observe(tutorial::Event::kJumpPerformed));
    assert(tutorial::snapshot().rejected_event_count == 1);
    assert(tutorial::observe(tutorial::Event::kDialogueSequenceComplete));
    assert(tutorial::snapshot().step == tutorial::Step::kRun);

    auto prompt = tutorial::current_prompt();
    assert(std::strcmp(prompt.title, "Run") == 0);
    assert(prompt.target == tutorial::Target::kJoystick);
    assert(std::strcmp(prompt.instruction, "Double-tap a direction to run.") == 0);

    // Each step advances only on the observed gameplay outcome, not merely a
    // button press or arbitrary timeout.
    assert(!tutorial::observe(tutorial::Event::kJumpPerformed));
    assert(tutorial::observe(tutorial::Event::kRunPerformed));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);
    assert(tutorial::current_prompt().target == tutorial::Target::kButtonC);

    assert(tutorial::observe(tutorial::Event::kJumpPerformed));
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
    assert(tutorial::current_prompt().target == tutorial::Target::kButtonA);

    // Attack waits for the tutorial enemy group to be cleared; the first A
    // press alone is intentionally not a state transition in this contract.
    assert(!tutorial::observe(tutorial::Event::kSuccessfulCounterAttack));
    assert(tutorial::observe(tutorial::Event::kTutorialEnemiesDefeated));
    assert(tutorial::snapshot().step == tutorial::Step::kBlock);
    assert(tutorial::current_prompt().target == tutorial::Target::kButtonB);

    assert(tutorial::observe(tutorial::Event::kSuccessfulBlock));
    assert(tutorial::snapshot().step == tutorial::Step::kCounterAttack);
    assert(tutorial::current_prompt().target == tutorial::Target::kButtonA);

    assert(tutorial::observe(tutorial::Event::kSuccessfulCounterAttack));
    assert(tutorial::snapshot().step == tutorial::Step::kDodge);
    assert(tutorial::current_prompt().target == tutorial::Target::kButtonB);

    assert(tutorial::observe(tutorial::Event::kSuccessfulDodge));
    assert(tutorial::snapshot().step == tutorial::Step::kAirAttack);
    assert(tutorial::current_prompt().target == tutorial::Target::kButtonC);

    assert(tutorial::observe(tutorial::Event::kTutorialBatDefeatedByAirAttack));
    assert(tutorial::snapshot().step == tutorial::Step::kFillRage);
    assert(tutorial::current_prompt().target == tutorial::Target::kItemSlot);

    assert(tutorial::observe(tutorial::Event::kTutorialItemFilledRage));
    assert(tutorial::snapshot().step == tutorial::Step::kExecution);
    assert(tutorial::current_prompt().target == tutorial::Target::kSwipeAtoC);

    assert(tutorial::observe(tutorial::Event::kSuccessfulExecution));
    state = tutorial::snapshot();
    assert(state.step == tutorial::Step::kComplete);
    assert(!state.active);
    assert(state.completed);
    assert(state.transition_count == 10);
    assert(!tutorial::observe(tutorial::Event::kRunPerformed));

    tutorial::begin_dialogue_sequence();
    state = tutorial::snapshot();
    assert(state.generation == 2);
    assert(state.step == tutorial::Step::kDialogue);
    assert(state.transition_count == 0);
    assert(state.rejected_event_count == 0);

    std::cout << "tutorial gameplay state smoke: ok\n";
    return 0;
}
