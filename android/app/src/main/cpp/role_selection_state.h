#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "login_callback_payload.h"

namespace nevergone::role_selection_state {

struct EnterRoleRequest {
    bool valid = false;
    std::int64_t character_id = 0;
    std::int64_t career = 0;
    std::string character_name;
};

struct CreateRoleRequest {
    bool valid = false;
    std::string character_name;
    std::int64_t career = 0;
};

struct Snapshot {
    bool payload_valid = false;
    std::uint64_t payload_generation = 0;
    std::size_t role_count = 0;
    int selected_index = -1;
    std::int64_t selected_career = 0;
    std::int64_t selected_character_id = 0;
    std::string selected_character_name;
    std::uint64_t selection_changes = 0;
    std::uint64_t confirm_count = 0;
    std::uint64_t create_request_count = 0;
    bool enter_request_pending = false;
    bool create_request_pending = false;
};

void reset();
void sync_role_list(const login_callback_payload::RoleListPayload& payload);

// Recovered online ChooseHero items use Career as their UI tag. Starting a
// selected role resolves the first role entry carrying that career and then
// dispatches its CharacterID to g_UILogin.EnterGameWithCid(CharacterID).
bool select_career(std::int64_t career);
bool select_index(int index);

EnterRoleRequest confirm_selection();
EnterRoleRequest peek_pending_enter_request();
EnterRoleRequest take_pending_enter_request();

// Recovered online create-role call shape:
// g_UILogin.CreateCharacter(name, career).
bool request_create_role(const std::string& character_name, std::int64_t career);
CreateRoleRequest peek_pending_create_request();
CreateRoleRequest take_pending_create_request();

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::role_selection_state
