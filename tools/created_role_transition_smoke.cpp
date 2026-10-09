#include <cassert>
#include <iostream>
#include <string>

#include "created_role_transition.h"

namespace {

int g_attempts = 0;
std::int64_t g_last_id = 0;

bool fail_dispatch(
        const nevergone::login_callback_payload::RoleEntry& role,
        std::string* error) {
    ++g_attempts;
    g_last_id = role.character_id;
    if (error != nullptr) *error = "synthetic enter failure";
    return false;
}

bool succeed_dispatch(
        const nevergone::login_callback_payload::RoleEntry& role,
        std::string* error) {
    ++g_attempts;
    g_last_id = role.character_id;
    if (error != nullptr) error->clear();
    return true;
}

}  // namespace

int main() {
    using namespace nevergone::created_role_transition;
    using nevergone::login_callback_payload::RoleEntry;

    reset();
    RoleEntry invalid;
    assert(!stage(invalid));
    auto state = snapshot();
    assert(!state.pending);
    assert(!state.last_error.empty());

    RoleEntry created;
    created.character_id = 303;
    created.character_name = "Nova";
    created.career = 5;
    created.character_level = 1;
    assert(stage(created));
    state = snapshot();
    assert(state.pending);
    assert(state.dispatch_due);
    assert(state.stage_count == 1);
    assert(state.role.character_id == 303);

    std::string error;
    assert(pump_with_dispatch(&fail_dispatch, &error) == Outcome::kDispatchFailed);
    assert(g_attempts == 1);
    assert(g_last_id == 303);
    assert(error == "synthetic enter failure");
    state = snapshot();
    assert(state.pending);
    assert(!state.dispatch_due);
    assert(state.dispatch_failure_count == 1);

    // A render loop or repeated caller cannot automatically hammer dispatch.
    assert(pump_with_dispatch(&succeed_dispatch, &error) == Outcome::kIdle);
    assert(g_attempts == 1);

    assert(request_retry());
    assert(!request_retry());
    assert(pump_with_dispatch(&succeed_dispatch, &error) == Outcome::kDispatched);
    assert(g_attempts == 2);
    assert(error.empty());
    state = snapshot();
    assert(!state.pending);
    assert(!state.dispatch_due);
    assert(state.dispatch_count == 1);
    assert(state.dispatch_failure_count == 1);
    assert(!request_retry());

    // A later server callback cleanly replaces the completed role.
    created.character_id = 404;
    created.character_name = "Rin";
    created.career = 2;
    assert(stage(created));
    assert(pump_with_dispatch(&succeed_dispatch, &error) == Outcome::kDispatched);
    assert(g_last_id == 404);
    state = snapshot();
    assert(state.stage_count == 2);
    assert(state.dispatch_count == 2);

    std::cout << "created role transition smoke: ok\n";
    return 0;
}
