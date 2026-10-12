#include <cassert>
#include <string>

#include "tutorial_gameplay_state.h"
#include "tutorial_session_state.h"

namespace session = nevergone::tutorial_session_state;
namespace gameplay = nevergone::tutorial_gameplay_state;

int main() {
    session::reset();
    assert(session::snapshot().phase == session::Phase::kInactive);
    assert(!session::modal_input_blocked());

    session::begin_loading();
    auto state = session::snapshot();
    assert(state.phase == session::Phase::kLoading);
    assert(state.generation == 1);
    assert(session::modal_input_blocked());
    assert(gameplay::snapshot().step == gameplay::Step::kInactive);

    assert(!session::on_dialogue_complete());
    assert(session::snapshot().rejected_transition_count == 1);

    assert(session::on_loading_complete());
    assert(session::snapshot().phase == session::Phase::kLessonOverlay);
    assert(session::modal_input_blocked());

    assert(session::on_lesson_overlay_complete());
    assert(session::snapshot().phase == session::Phase::kDialogue);
    assert(gameplay::snapshot().step == gameplay::Step::kDialogue);
    assert(session::modal_input_blocked());

    assert(session::on_dialogue_complete());
    assert(session::snapshot().phase == session::Phase::kInteractive);
    assert(gameplay::snapshot().step == gameplay::Step::kRun);
    assert(!session::modal_input_blocked());

    assert(!session::on_interactive_complete());
    assert(session::snapshot().phase == session::Phase::kInteractive);

    assert(gameplay::observe(gameplay::Event::kRunPerformed));
    assert(gameplay::observe(gameplay::Event::kJumpPerformed));
    assert(gameplay::observe(gameplay::Event::kTutorialEnemiesDefeated));
    assert(gameplay::observe(gameplay::Event::kSuccessfulBlock));
    assert(gameplay::observe(gameplay::Event::kSuccessfulCounterAttack));
    assert(gameplay::observe(gameplay::Event::kSuccessfulDodge));
    assert(gameplay::observe(gameplay::Event::kTutorialBatDefeatedByAirAttack));
    assert(gameplay::observe(gameplay::Event::kTutorialItemFilledRage));
    assert(gameplay::observe(gameplay::Event::kSuccessfulExecution));
    assert(gameplay::snapshot().completed);

    assert(session::on_interactive_complete());
    assert(session::snapshot().phase == session::Phase::kComplete);
    assert(!session::modal_input_blocked());

    session::begin_loading();
    assert(session::snapshot().generation == 2);
    assert(gameplay::snapshot().step == gameplay::Step::kInactive);
    return 0;
}
