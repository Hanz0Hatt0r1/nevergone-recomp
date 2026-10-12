#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "character_name_state.h"
#include "role_selection_state.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"
#include "startup_contract.h"

int main() {
    namespace fs = std::filesystem;
    namespace rune_layout = nevergone::single_select_hero_rune_layout;

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
    assert(rune_layout::selected_render_tag() == 0);

    for (std::int64_t career = 1; career <= 5; ++career) {
        assert(nevergone::single_select_hero_state::is_valid_career(career));
    }
    assert(!nevergone::single_select_hero_state::is_valid_career(0));
    assert(!nevergone::single_select_hero_state::is_valid_career(6));

    nevergone::single_select_hero_state::begin(0);
    auto selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.active);
    assert(selector.existing_career == 0);
    assert(selector.selected_career == 1);
    assert(rune_layout::selected_render_tag() == 1);
    assert(rune_layout::frame_index(1, false) == 1);
    assert(rune_layout::frame_index(2, false) == 2);
    assert(!selector.input_enabled);
    assert(selector.transition_pending);
    assert(selector.selection_count == 0);
    assert(!nevergone::single_select_hero_state::select_career(2));

    assert(nevergone::single_select_hero_state::confirm_online());
    auto character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active);
    assert(character_name.career == 1);
    assert(character_name.randomize_pending);

    nevergone::single_select_hero_state::complete_transition();
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.input_enabled);
    assert(!selector.transition_pending);

    assert(nevergone::single_select_hero_state::select_career(1));
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.selection_count == 0);
    assert(rune_layout::selected_render_tag() == 1);

    assert(nevergone::single_select_hero_state::select_career(2));
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.selected_career == 2);
    assert(rune_layout::selected_render_tag() == 2);
    assert(rune_layout::frame_index(1, false) == 0);
    assert(rune_layout::frame_index(2, false) == 3);
    assert(!selector.input_enabled);
    assert(selector.transition_pending);
    assert(selector.selection_count == 1);
    nevergone::single_select_hero_state::complete_transition();

    // Careers 3..5 remain semantic sender-tag values in the recovered state
    // machine, but the captured shipped screen has no visible rune slot for
    // them. Their selection therefore clears persistent rune glow rather than
    // presenting a hidden control.
    for (std::int64_t career = 3; career <= 5; ++career) {
        assert(nevergone::single_select_hero_state::select_career(career));
        selector = nevergone::single_select_hero_state::snapshot();
        assert(selector.selected_career == career);
        assert(rune_layout::selected_render_tag() == 0);
        assert(!selector.input_enabled);
        assert(selector.transition_pending);
        assert(selector.selection_count == static_cast<std::uint64_t>(career - 1));
        nevergone::single_select_hero_state::complete_transition();
    }
    assert(!nevergone::single_select_hero_state::select_career(6));

    nevergone::character_name_state::reset();
    assert(nevergone::single_select_hero_state::confirm_online());
    character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active);
    assert(character_name.career == 5);
    assert(character_name.randomize_pending);

    // Existing career 1 starts on career 2; the second runtime capture (Higgs)
    // proves this must persistently select the tag-2 faguang presentation.
    nevergone::character_name_state::reset();
    nevergone::single_select_hero_state::begin(1);
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.existing_career == 1);
    assert(selector.selected_career == 2);
    assert(rune_layout::selected_render_tag() == 2);
    assert(rune_layout::frame_index(1, false) == 0);
    assert(rune_layout::frame_index(2, false) == 3);
    assert(!selector.input_enabled);
    assert(nevergone::single_select_hero_state::confirm_online());
    character_name = nevergone::character_name_state::snapshot();
    assert(character_name.active);
    assert(character_name.career == 2);

    for (std::int64_t existing = 2; existing <= 5; ++existing) {
        nevergone::character_name_state::reset();
        nevergone::single_select_hero_state::begin(existing);
        selector = nevergone::single_select_hero_state::snapshot();
        assert(selector.existing_career == existing);
        assert(selector.selected_career == 1);
        assert(rune_layout::selected_render_tag() == 1);
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

    nevergone::single_select_hero_state::begin(99);
    selector = nevergone::single_select_hero_state::snapshot();
    assert(selector.existing_career == 0);
    assert(selector.selected_career == 1);
    assert(rune_layout::selected_render_tag() == 1);
    assert(!selector.input_enabled);
    assert(selector.transition_pending);

    nevergone::single_select_hero_state::reset();
    assert(rune_layout::selected_render_tag() == 0);

    fs::remove_all(root);
    std::cout << "single select hero state smoke: ok\n";
    return 0;
}
