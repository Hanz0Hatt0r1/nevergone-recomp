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

    for (std::int64_t career = 1; career <= 5; ++career) {
        assert(nevergone::single_select_hero_state::is_valid_career(career));
    }
    assert(!nevergone::single_select_hero_state::is_valid_career(0));
    assert(!nevergone::single_select_hero_state::is_valid_career(6));

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
    // visible career is a handled no-op; each changed career starts another
    // locked transition until OpenTheDoor(true) is represented by
    // complete_transition().
    assert(nevergone::single_select_hero_state::select_career(1));
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.selection_count == 0);

    for (std::int64_t career = 2; career <= 5; ++career) {
        assert(nevergone::single_select_hero_state::select_career(career));
        selector = nevergone::single_select_hero_state::snapshot();
        assert(selector.selected_career == career);
        assert(!selector.input_enabled);
        assert(selector.transition_pending);
        assert(selector.selection_count == static_cast<std::uint64_t>(career - 1));
        assert(!nevergone::single_select_hero_state::select_career(
            career == 5 ? 1 : career + 1));
        nevergone::single_select_hero_state::complete_transition();
    }
    assert(!nevergone::single_select_hero_state::select_career(6));

    // The currently selected career is forwarded unchanged into CharacterName.
    nevergone::character_name_state::reset();
    assert(nevergone::single_select_hero_state::confirm_online());
    character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active);
    assert(character_name.career == 5);
    assert(character_name.randomize_pending);

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

    // Existing careers 2..5 are preserved. The default candidate remains 1,
    // but menuOpenGC may move onto the existing career; menuConfirm owns the
    // recovered equality block and must reject it for every supported career.
    for (std::int64_t existing = 2; existing <= 5; ++existing) {
        nevergone::character_name_state::reset();
        nevergone::single_select_hero_state::begin(existing);
        selector = nevergone::single_select_hero_state::snapshot();
        assert(selector.existing_career == existing);
        assert(selector.selected_career == 1);
        nevergone::single_select_hero_state::complete_transition();
        assert(nevergone::single_select_hero_state::select_career(existing));
        assert(!nevergone::single_select_hero_state::confirm_online());
        selector = nevergone::single_select_hero_state::snapshot();
        assert(selector.confirm_count == 1);
        assert(selector.blocked_confirm_count == 1);
        assert(selector.character_name_open_count == 0);
        character_name = nevergone::character_name_state::snapshot();
        assert(!character_name.active);
    }

    // Values outside the shipped 1..5 sender-tag range are treated as no
    // existing supported career.
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
