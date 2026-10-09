#include "choose_hero_action_state.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    using nevergone::choose_hero_action_state::Action;
    using nevergone::choose_hero_action_state::Request;

    struct Mapping {
        int tag;
        Action action;
    };
    const Mapping mappings[] = {
        {1, Action::kOpenCreateHeroScreen},
        {2, Action::kLeaveToSelectHero},
        {3, Action::kStartSelectedHero},
        {4, Action::kSubmitCreateRole},
        {5, Action::kRandomizeRoleName},
        {6, Action::kDetachImeAndRestoreSaveList},
        {7, Action::kAttachIme},
        {8, Action::kConfirmDeleteSelectedHero},
    };

    for (const Mapping& mapping : mappings) {
        Action action = Action::kNone;
        assert(nevergone::choose_hero_action_state::action_from_tag(mapping.tag, &action));
        assert(action == mapping.action);
        assert(std::string(nevergone::choose_hero_action_state::action_name(action)) != "none");
    }

    Action invalid = Action::kStartSelectedHero;
    assert(!nevergone::choose_hero_action_state::action_from_tag(0, &invalid));
    assert(invalid == Action::kNone);
    assert(!nevergone::choose_hero_action_state::action_from_tag(9, &invalid));
    assert(!nevergone::choose_hero_action_state::action_from_tag(1, nullptr));

    nevergone::choose_hero_action_state::reset(17);
    auto state = nevergone::choose_hero_action_state::snapshot();
    assert(state.scene_generation == 17);
    assert(!state.pending);
    assert(state.dispatch_count == 0);
    assert(state.consume_count == 0);

    assert(nevergone::choose_hero_action_state::dispatch(3, 17, 2, 0));
    Request request;
    assert(nevergone::choose_hero_action_state::peek(&request));
    assert(request.action == Action::kStartSelectedHero);
    assert(request.scene_generation == 17);
    assert(request.sequence == 1);
    assert(request.selected_hero_id == 2);
    assert(request.role_name.empty());

    // A later UI action replaces the single pending request while preserving
    // the scene-local sequence number.
    assert(nevergone::choose_hero_action_state::dispatch(4, 17, 2, 1, "HeroName"));
    assert(nevergone::choose_hero_action_state::peek(&request));
    assert(request.action == Action::kSubmitCreateRole);
    assert(request.sequence == 2);
    assert(request.create_role_parameter == 1);
    assert(request.role_name == "HeroName");

    assert(nevergone::choose_hero_action_state::consume(&request));
    assert(request.action == Action::kSubmitCreateRole);
    assert(!nevergone::choose_hero_action_state::peek(&request));
    state = nevergone::choose_hero_action_state::snapshot();
    assert(!state.pending);
    assert(state.dispatch_count == 2);
    assert(state.consume_count == 1);

    // A new scene generation discards stale pending work and restarts the
    // scene-local sequence counter.
    assert(nevergone::choose_hero_action_state::dispatch(8, 18, 1, 0));
    state = nevergone::choose_hero_action_state::snapshot();
    assert(state.scene_generation == 18);
    assert(state.dispatch_count == 1);
    assert(state.consume_count == 0);
    assert(state.pending);
    assert(state.latest.action == Action::kConfirmDeleteSelectedHero);
    assert(state.latest.selected_hero_id == 1);

    assert(!nevergone::choose_hero_action_state::dispatch(42, 18, 1, 0));
    state = nevergone::choose_hero_action_state::snapshot();
    assert(state.dispatch_count == 1);
    assert(state.latest.action == Action::kConfirmDeleteSelectedHero);

    std::cout << nevergone::choose_hero_action_state::status_report();
    return 0;
}
