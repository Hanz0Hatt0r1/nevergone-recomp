#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "character_name_action_executor.h"
#include "character_name_state.h"
#include "role_selection_state.h"
#include "startup_contract.h"

namespace {

int g_create_attempts = 0;
int g_randomize_attempts = 0;

bool fail_create_dispatch() {
    ++g_create_attempts;
    return false;
}

bool succeed_create_dispatch() {
    ++g_create_attempts;
    const auto request = nevergone::role_selection_state::take_pending_create_request();
    return request.valid;
}

bool fail_randomize(std::string* error) {
    ++g_randomize_attempts;
    if (error != nullptr) *error = "synthetic random-name failure";
    return false;
}

bool succeed_randomize(std::string* error) {
    ++g_randomize_attempts;
    std::int64_t career = 0;
    if (!nevergone::character_name_state::take_randomize_request(&career) || career != 2) {
        if (error != nullptr) *error = "pending random-name request mismatch";
        return false;
    }
    if (!nevergone::character_name_state::set_role_name("RandomHero")) {
        if (error != nullptr) *error = "failed to apply synthetic random name";
        return false;
    }
    if (error != nullptr) error->clear();
    return true;
}

}  // namespace

int main() {
    using nevergone::character_name_action_executor::Outcome;
    using nevergone::character_name_action_executor::dispatch_tag_with_callbacks;

    nevergone::startup::RuntimeConfig config;
    config.files_dir = "/tmp/nevergone-character-name-executor-smoke";
    nevergone::startup::configure(config);

    nevergone::role_selection_state::reset();
    nevergone::character_name_state::reset();

    std::string error;
    assert(dispatch_tag_with_callbacks(99, &succeed_create_dispatch, &succeed_randomize, &error) ==
           Outcome::kInvalidTag);
    assert(!error.empty());

    assert(dispatch_tag_with_callbacks(1, &succeed_create_dispatch, &succeed_randomize, &error) ==
           Outcome::kInactive);

    nevergone::character_name_state::begin(2);
    assert(nevergone::character_name_state::set_role_name(""));
    assert(dispatch_tag_with_callbacks(1, &succeed_create_dispatch, &succeed_randomize, &error) ==
           Outcome::kValidationRejected);
    assert(g_create_attempts == 0);
    assert(!nevergone::role_selection_state::peek_pending_create_request().valid);

    assert(nevergone::character_name_state::set_role_name("HeroOne"));
    assert(dispatch_tag_with_callbacks(1, &fail_create_dispatch, &succeed_randomize, &error) ==
           Outcome::kCreateDispatchFailed);
    assert(g_create_attempts == 1);
    auto pending = nevergone::role_selection_state::peek_pending_create_request();
    assert(pending.valid);
    assert(pending.career == 2);
    assert(pending.character_name == "HeroOne");

    assert(nevergone::character_name_state::set_role_name("HeroTwo"));
    assert(dispatch_tag_with_callbacks(1, &succeed_create_dispatch, &succeed_randomize, &error) ==
           Outcome::kSubmitted);
    assert(g_create_attempts == 2);
    assert(!nevergone::role_selection_state::peek_pending_create_request().valid);

    assert(dispatch_tag_with_callbacks(3, &succeed_create_dispatch, &fail_randomize, &error) ==
           Outcome::kRandomizeFailed);
    assert(g_randomize_attempts == 1);
    assert(nevergone::character_name_state::randomize_pending());

    assert(dispatch_tag_with_callbacks(3, &succeed_create_dispatch, &succeed_randomize, &error) ==
           Outcome::kRandomized);
    assert(g_randomize_attempts == 2);
    auto state = nevergone::character_name_state::snapshot();
    assert(state.role_name == "RandomHero");
    assert(!state.randomize_pending);

    assert(dispatch_tag_with_callbacks(2, &succeed_create_dispatch, &succeed_randomize, &error) ==
           Outcome::kClosed);
    assert(!nevergone::character_name_state::snapshot().active);

    assert(dispatch_tag_with_callbacks(3, &succeed_create_dispatch, &succeed_randomize, &error) ==
           Outcome::kInactive);

    std::cout << "character name action executor smoke: ok\n";
    return 0;
}
