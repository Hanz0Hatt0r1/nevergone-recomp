#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "character_name_state.h"
#include "role_selection_state.h"
#include "single_select_hero_state.h"
#include "startup_contract.h"

int main() {
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "nevergone_single_select_hero_state";
    fs::remove_all(root);
    fs::create_directories(root / "assets");
    {
        std::ofstream dictionary(root / "assets" / "newWord.txt", std::ios::binary);
        dictionary << "blocked\n";
    }
    nevergone::startup::RuntimeConfig config;
    config.files_dir = root.string();
    nevergone::startup::configure(config);

    nevergone::role_selection_state::reset();
    nevergone::character_name_state::reset();
    nevergone::single_select_hero_state::reset();

    assert(nevergone::single_select_hero_state::is_valid_career(1));
    assert(nevergone::single_select_hero_state::is_valid_career(2));
    assert(!nevergone::single_select_hero_state::is_valid_career(0));
    assert(!nevergone::single_select_hero_state::is_valid_career(3));

    // With no existing role the shipped initUI path starts on career 1 and
    // disables menuOpenGC input while the initial Carousel transition runs.
    nevergone::single_select_hero_state::begin(0);
    auto selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.active);
    assert(selector.existing_career == 0);
    assert(selector.selected_career == 1);
    assert(!selector.input_enabled);
    assert(selector.transition_pending);
    assert(selector.selection_count == 0);
    assert(!nevergone::single_select_hero_state::select_career(2));

    // menuConfirm itself has no menuOpenGC gate. The exact selected career may
    // proceed online even while the visual transition flag is locked.
    assert(nevergone::single_select_hero_state::confirm_online());
    auto character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active);
    assert(character_name.career == 1);
    assert(character_name.randomize_pending);

    nevergone::single_select_hero_state::complete_transition();
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.input_enabled);
    assert(!selector.transition_pending);

    // menuOpenGC uses the sender tag directly as career. Re-selecting the
    // visible career is a handled no-op; changing it starts a new locked
    // transition until OpenTheDoor(true) is represented by complete_transition.
    assert(nevergone::single_select_hero_state::select_career(1));
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.selection_count == 0);
    assert(nevergone::single_select_hero_state::select_career(2));
    assert(!nevergone::single_select_hero_state::select_career(3));
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.selected_career == 2);
    assert(!selector.input_enabled);
    assert(selector.transition_pending);
    assert(selector.selection_count == 1);
    assert(!nevergone::single_select_hero_state::select_career(1));

    nevergone::single_select_hero_state::complete_transition();
    nevergone::character_name_state::reset();
    assert(nevergone::single_select_hero_state::confirm_online());
    character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active);
    assert(character_name.career == 2);
    assert(character_name.randomize_pending);
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.confirm_count == 2);
    assert(selector.blocked_confirm_count == 0);
    assert(selector.character_name_open_count == 2);

    // If career 1 already exists, initUI deliberately starts on career 2.
    nevergone::character_name_state::reset();
    nevergone::single_select_hero_state::begin(1);
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.existing_career == 1);
    assert(selector.selected_career == 2);
    assert(!selector.input_enabled);
    assert(nevergone::single_select_hero_state::confirm_online());
    character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active);
    assert(character_name.career == 2);

    // If career 2 exists, the default remains career 1. After the transition
    // gate opens, menuOpenGC may move back onto career 2; menuConfirm owns the
    // recovered equality check and blocks CharacterNameLayer in that case.
    nevergone::character_name_state::reset();
    nevergone::single_select_hero_state::begin(2);
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.existing_career == 2);
    assert(selector.selected_career == 1);
    nevergone::single_select_hero_state::complete_transition();
    assert(nevergone::single_select_hero_state::select_career(2));
    assert(!nevergone::single_select_hero_state::confirm_online());
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.confirm_count == 1);
    assert(selector.blocked_confirm_count == 1);
    assert(selector.character_name_open_count == 0);
    character_name = nevergone::character_name_state::snapshot();
    assert(!character_name.active);

    // Non-career values in the existing-role field are equivalent to no
    // existing supported career for this two-class selector.
    nevergone::single_select_hero_state::begin(99);
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.existing_career == 0);
    assert(selector.selected_career == 1);
    assert(!selector.input_enabled);
    assert(selector.transition_pending);

    fs::remove_all(root);
    std::cout << "single select hero state smoke: ok\n";
    return 0;
}
