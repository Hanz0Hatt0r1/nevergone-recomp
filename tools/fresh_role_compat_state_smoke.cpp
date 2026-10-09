#include <cassert>
#include <iostream>
#include <string>

#include "character_name_state.h"
#include "fresh_role_compat_state.h"
#include "single_select_hero_state.h"

int main() {
    using nevergone::fresh_role_compat_state::Mode;

    nevergone::character_name_state::reset();
    nevergone::single_select_hero_state::reset();

    auto compat = nevergone::fresh_role_compat_state::snapshot();
    assert(compat.mode == Mode::kHidden);

    nevergone::single_select_hero_state::begin(0);
    compat = nevergone::fresh_role_compat_state::snapshot();
    assert(compat.mode == Mode::kCareerSelection);
    assert(compat.selected_career == 1);
    assert(!compat.input_enabled);
    assert(compat.transition_pending);

    // The compatibility input deliberately collapses the still-missing door
    // transition before and after a changed-career tap.
    assert(!nevergone::fresh_role_compat_state::select_career(3));
    assert(nevergone::fresh_role_compat_state::select_career(2));
    compat = nevergone::fresh_role_compat_state::snapshot();
    assert(compat.selected_career == 2);
    assert(compat.input_enabled);
    assert(!compat.transition_pending);

    assert(nevergone::fresh_role_compat_state::confirm_career());
    compat = nevergone::fresh_role_compat_state::snapshot();
    assert(compat.mode == Mode::kCharacterName);
    auto name = nevergone::character_name_state::snapshot();
    assert(name.active);
    assert(name.career == 2);
    assert(name.randomize_pending);

    assert(nevergone::fresh_role_compat_state::set_role_name("CompatHero"));
    compat = nevergone::fresh_role_compat_state::snapshot();
    assert(compat.role_name == "CompatHero");

    // CharacterNameLayer remains the top semantic surface even if the selector
    // beneath it is reset; once both are inactive the shim disappears.
    nevergone::single_select_hero_state::reset();
    assert(nevergone::fresh_role_compat_state::snapshot().mode == Mode::kCharacterName);
    nevergone::character_name_state::reset();
    assert(nevergone::fresh_role_compat_state::snapshot().mode == Mode::kHidden);

    // Existing-career equality remains owned by recovered menuConfirm(). The
    // compatibility selector must not bypass that semantic guard.
    nevergone::single_select_hero_state::begin(1);
    assert(nevergone::fresh_role_compat_state::select_career(1));
    assert(!nevergone::fresh_role_compat_state::confirm_career());
    assert(!nevergone::character_name_state::snapshot().active);

    assert(std::string(nevergone::fresh_role_compat_state::mode_name(
               Mode::kCareerSelection)) == "career-selection");

    std::cout << "fresh-role compatibility state smoke: ok\n";
    return 0;
}
