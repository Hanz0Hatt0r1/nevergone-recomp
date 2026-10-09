#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "character_name_state.h"
#include "role_creation_validation.h"
#include "role_selection_state.h"
#include "startup_contract.h"

int main() {
    namespace fs = std::filesystem;
    using nevergone::character_name_state::Action;
    using nevergone::role_creation_validation::Result;

    const fs::path root = fs::temp_directory_path() / "nevergone_character_name_state";
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

    Action action = Action::kNone;
    assert(nevergone::character_name_state::action_from_tag(1, &action));
    assert(action == Action::kSubmit);
    assert(nevergone::character_name_state::action_from_tag(2, &action));
    assert(action == Action::kClose);
    assert(nevergone::character_name_state::action_from_tag(3, &action));
    assert(action == Action::kRandomize);
    assert(!nevergone::character_name_state::action_from_tag(4, &action));

    nevergone::character_name_state::begin(1);
    auto state = nevergone::character_name_state::snapshot();
    assert(state.active);
    assert(state.career == 1);
    assert(state.randomize_pending);
    assert(state.randomize_count == 1);

    std::int64_t randomize_career = 0;
    assert(nevergone::character_name_state::take_randomize_request(&randomize_career));
    assert(randomize_career == 1);
    assert(!nevergone::character_name_state::randomize_pending());

    // CharacterNameLayer has its own recovered 18-byte boundary, distinct from
    // the legacy ChooseHero tag-4 path which accepts up to 21 bytes.
    assert(nevergone::character_name_state::set_role_name("123456789012345678"));
    assert(nevergone::character_name_state::dispatch_tag(1));
    auto request = nevergone::role_selection_state::peek_pending_create_request();
    assert(request.valid);
    assert(request.character_name == "123456789012345678");
    assert(request.career == 1);
    state = nevergone::character_name_state::snapshot();
    assert(state.last_validation.result == Result::kValid);
    assert(state.last_validation.byte_length == 18);
    nevergone::role_selection_state::take_pending_create_request();

    assert(nevergone::character_name_state::set_role_name("1234567890123456789"));
    assert(nevergone::character_name_state::dispatch_tag(1));
    request = nevergone::role_selection_state::peek_pending_create_request();
    assert(!request.valid);
    state = nevergone::character_name_state::snapshot();
    assert(state.last_validation.result == Result::kTooLong);
    assert(state.last_validation.byte_length == 19);

    assert(nevergone::character_name_state::set_role_name("xxblockedyy"));
    assert(nevergone::character_name_state::dispatch_tag(1));
    request = nevergone::role_selection_state::peek_pending_create_request();
    assert(!request.valid);
    state = nevergone::character_name_state::snapshot();
    assert(state.last_validation.result == Result::kBlockedByDictionary);

    assert(nevergone::character_name_state::dispatch_tag(3));
    assert(nevergone::character_name_state::randomize_pending());
    assert(nevergone::character_name_state::take_randomize_request(nullptr));

    assert(nevergone::character_name_state::dispatch_tag(2));
    state = nevergone::character_name_state::snapshot();
    assert(!state.active);
    assert(state.close_count == 1);
    assert(!nevergone::character_name_state::dispatch_tag(1));

    // Career 2 is preserved identically through the CharacterNameLayer state
    // into the existing online create-role request contract.
    nevergone::character_name_state::begin(2);
    assert(nevergone::character_name_state::set_role_name("HeroTwo"));
    assert(nevergone::character_name_state::dispatch_tag(1));
    request = nevergone::role_selection_state::peek_pending_create_request();
    assert(request.valid);
    assert(request.character_name == "HeroTwo");
    assert(request.career == 2);

    fs::remove_all(root);
    std::cout << "character name state smoke: ok\n";
    return 0;
}
