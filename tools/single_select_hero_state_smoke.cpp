#include <cassert>
#include <iostream>

#include "single_select_hero_state.h"

int main() {
    namespace state = nevergone::single_select_hero_state;

    assert(state::valid_career(1));
    assert(state::valid_career(2));
    assert(!state::valid_career(0));
    assert(!state::valid_career(3));

    state::reset();
    auto snapshot = state::snapshot();
    assert(!snapshot.active);

    // With no unavailable existing value, initUI selects career 1 and enters
    // the initial Carousel transition with interaction locked.
    state::begin(0);
    snapshot = state::snapshot();
    const auto first_generation = snapshot.generation;
    assert(snapshot.active);
    assert(snapshot.unavailable_career == 0);
    assert(snapshot.current_career == 1);
    assert(snapshot.selected_career == 1);
    assert(!snapshot.input_enabled);
    assert(snapshot.transition_pending);
    assert(!state::select_career(2));

    // menuConfirm itself is not guarded by the menuOpenGC interaction gate.
    assert(state::confirm());
    state::ConfirmRequest request;
    assert(state::peek_confirm(&request));
    assert(request.valid);
    assert(request.generation == first_generation);
    assert(request.sequence == 1);
    assert(request.career == 1);

    state::complete_transition();
    snapshot = state::snapshot();
    assert(snapshot.input_enabled);
    assert(!snapshot.transition_pending);

    assert(!state::select_career(1));
    assert(!state::select_career(0));
    assert(state::select_career(2));
    snapshot = state::snapshot();
    assert(snapshot.current_career == 2);
    assert(snapshot.selected_career == 2);
    assert(!snapshot.input_enabled);
    assert(snapshot.transition_pending);
    assert(snapshot.selection_count == 1);
    assert(!state::peek_confirm(&request));

    state::complete_transition();
    assert(state::confirm());
    assert(state::take_confirm(&request));
    assert(request.career == 2);
    snapshot = state::snapshot();
    assert(snapshot.confirm_count == 2);
    assert(snapshot.consume_count == 1);
    assert(!snapshot.pending_confirm.valid);

    // If career 1 is already represented by the existing value, initUI starts
    // on career 2 and the unavailable career cannot be selected later.
    state::begin(1);
    snapshot = state::snapshot();
    assert(snapshot.generation != first_generation);
    assert(snapshot.unavailable_career == 1);
    assert(snapshot.current_career == 2);
    assert(snapshot.selected_career == 2);
    state::complete_transition();
    assert(!state::select_career(1));
    assert(state::confirm());
    assert(state::peek_confirm(&request));
    assert(request.career == 2);

    // The symmetrical case keeps career 1 available/default.
    state::begin(2);
    snapshot = state::snapshot();
    assert(snapshot.unavailable_career == 2);
    assert(snapshot.current_career == 1);
    assert(snapshot.selected_career == 1);
    assert(!state::peek_confirm(&request));  // new generation invalidates stale confirm
    state::complete_transition();
    assert(!state::select_career(2));
    assert(state::confirm());
    assert(state::take_confirm(&request));
    assert(request.career == 1);

    // Values outside the recovered 1/2 domain are treated as no unavailable
    // career rather than inventing a third class.
    state::begin(99);
    snapshot = state::snapshot();
    assert(snapshot.unavailable_career == 0);
    assert(snapshot.current_career == 1);

    std::cout << "SingleSelectHero career state smoke: ok\n";
    return 0;
}
