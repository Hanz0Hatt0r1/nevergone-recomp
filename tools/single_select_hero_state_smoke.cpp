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
    { std::ofstream dictionary(root / "assets" / "newWord.txt", std::ios::binary); dictionary << "blocked\n"; }
    nevergone::startup::RuntimeConfig config; config.files_dir = root.string(); nevergone::startup::configure(config);
    nevergone::role_selection_state::reset();
    nevergone::character_name_state::reset();
    nevergone::single_select_hero_state::reset();

    for (std::int64_t career = 1; career <= 5; ++career) assert(nevergone::single_select_hero_state::is_valid_career(career));
    assert(!nevergone::single_select_hero_state::is_valid_career(0));
    assert(!nevergone::single_select_hero_state::is_valid_career(6));

    nevergone::single_select_hero_state::begin(0);
    auto selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.existing_career == 0 && selector.selected_career == 1);
    assert(!selector.input_enabled && selector.transition_pending);
    assert(nevergone::single_select_hero_state::confirm_online());
    auto character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active && character_name.career == 1);

    nevergone::single_select_hero_state::complete_transition();
    for (std::int64_t career = 2; career <= 5; ++career) {
        assert(nevergone::single_select_hero_state::select_career(career));
        selector = nevergone::single_select_hero_state::snapshot();
        assert(selector.selected_career == career);
        assert(!selector.input_enabled && selector.transition_pending);
        nevergone::single_select_hero_state::complete_transition();
    }
    assert(!nevergone::single_select_hero_state::select_career(6));
    nevergone::character_name_state::reset();
    assert(nevergone::single_select_hero_state::confirm_online());
    character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active && character_name.career == 5);

    nevergone::character_name_state::reset();
    nevergone::single_select_hero_state::begin(1);
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.existing_career == 1 && selector.selected_career == 2);

    for (std::int64_t existing = 2; existing <= 5; ++existing) {
        nevergone::character_name_state::reset();
        nevergone::single_select_hero_state::begin(existing);
        selector = nevergone::single_select_hero_state::snapshot();
        assert(selector.existing_career == existing && selector.selected_career == 1);
        nevergone::single_select_hero_state::complete_transition();
        assert(nevergone::single_select_hero_state::select_career(existing));
        assert(!nevergone::single_select_hero_state::confirm_online());
        selector = nevergone::single_select_hero_state::snapshot();
        assert(selector.blocked_confirm_count == 1 && selector.character_name_open_count == 0);
        assert(!nevergone::character_name_state::snapshot().active);
    }

    nevergone::single_select_hero_state::begin(99);
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.existing_career == 0 && selector.selected_career == 1);

    fs::remove_all(root);
    std::cout << "single select hero state smoke: ok\n";
    return 0;
}
