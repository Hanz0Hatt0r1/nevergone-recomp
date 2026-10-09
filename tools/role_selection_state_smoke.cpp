#include <cassert>
#include <iostream>

#include "role_selection_state.h"

using nevergone::login_callback_payload::RoleEntry;
using nevergone::login_callback_payload::RoleListPayload;
using namespace nevergone::role_selection_state;

int main() {
    reset();
    auto state = snapshot();
    assert(!state.payload_valid);
    assert(state.selected_index == -1);
    assert(!state.enter_request_pending);
    assert(!state.create_request_pending);

    RoleListPayload payload;
    payload.valid = true;
    payload.roles = {
        RoleEntry{101, "Aria", 1, 12, 31, 4},
        RoleEntry{202, "Bram", 2, 7, 19, 3},
        RoleEntry{303, "Cato", 1, 21, 42, 8},
    };
    sync_role_list(payload);

    state = snapshot();
    assert(state.payload_valid);
    assert(state.role_count == 3);
    assert(state.selected_index == -1);

    // Recovered online UI tags roles by Career. Duplicate careers resolve to
    // the first matching role, matching the original start-path scan.
    assert(select_career(1));
    state = snapshot();
    assert(state.selected_index == 0);
    assert(state.selected_character_id == 101);
    assert(state.selected_character_name == "Aria");
    assert(state.selection_changes == 1);

    const auto enter = confirm_selection();
    assert(enter.valid);
    assert(enter.character_id == 101);
    assert(enter.career == 1);
    assert(peek_pending_enter_request().valid);

    // Changing selection invalidates the previous pending enter request.
    assert(select_index(1));
    assert(!peek_pending_enter_request().valid);
    state = snapshot();
    assert(state.selected_character_id == 202);
    assert(state.selection_changes == 2);

    const auto second_enter = confirm_selection();
    assert(second_enter.valid);
    assert(second_enter.character_id == 202);
    const auto consumed_enter = take_pending_enter_request();
    assert(consumed_enter.valid);
    assert(consumed_enter.character_id == 202);
    assert(!take_pending_enter_request().valid);

    assert(!select_career(99));
    assert(!select_index(-1));
    assert(!select_index(99));

    assert(!request_create_role("", 2));
    assert(request_create_role("Dara", 2));
    const auto create = peek_pending_create_request();
    assert(create.valid);
    assert(create.character_name == "Dara");
    assert(create.career == 2);
    assert(!peek_pending_enter_request().valid);
    const auto consumed_create = take_pending_create_request();
    assert(consumed_create.valid);
    assert(consumed_create.character_name == "Dara");
    assert(!take_pending_create_request().valid);

    // A fresh callback payload invalidates stale selection and requests.
    RoleListPayload empty;
    empty.valid = true;
    sync_role_list(empty);
    state = snapshot();
    assert(state.payload_valid);
    assert(state.role_count == 0);
    assert(state.selected_index == -1);
    assert(!confirm_selection().valid);

    reset();
    state = snapshot();
    assert(!state.payload_valid);
    assert(state.role_count == 0);
    assert(state.selection_changes == 0);
    assert(state.confirm_count == 0);
    assert(state.create_request_count == 0);

    std::cout << "role selection state smoke: ok\n";
    return 0;
}
