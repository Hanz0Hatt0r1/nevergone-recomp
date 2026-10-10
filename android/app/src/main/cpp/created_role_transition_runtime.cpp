#include "created_role_transition.h"

#include "initial_ui_transition.h"
#include "login_lua_dispatch.h"
#include "role_selection_state.h"

namespace nevergone::created_role_transition {
namespace {

thread_local lua_State* g_inline_state = nullptr;

bool inline_dispatch(
        const login_callback_payload::RoleEntry& role,
        std::string* error) {
    if (g_inline_state == nullptr) {
        if (error != nullptr) *error = "created-role Lua state unavailable";
        return false;
    }
    if (!role_selection_state::request_enter_role(role)) {
        if (error != nullptr) *error = "failed to stage created-role enter request";
        return false;
    }

    std::string dispatch_error;
    if (!login_lua_dispatch::call_enter_game_with_cid(
            g_inline_state, role.character_id, &dispatch_error)) {
        if (error != nullptr) {
            *error = dispatch_error.empty() ? "EnterGameWithCid dispatch failed" : dispatch_error;
        }
        return false;
    }

    const auto consumed = role_selection_state::take_pending_enter_request();
    if (!consumed.valid || consumed.character_id != role.character_id ||
            consumed.career != role.career || consumed.character_name != role.character_name) {
        if (error != nullptr) *error = "created-role enter request changed during dispatch";
        return false;
    }
    if (!role_selection_state::commit_enter_dispatch(consumed)) {
        if (error != nullptr) *error = "created-role enter generation changed during dispatch";
        return false;
    }
    if (!initial_ui_transition::on_role_enter_dispatch_succeeded()) {
        if (error != nullptr) *error = "created-role enter completed outside role-created route";
        return false;
    }

    if (error != nullptr) error->clear();
    return true;
}

}  // namespace

Outcome pump_inline(lua_State* state, std::string* error) {
    if (state == nullptr) {
        if (error != nullptr) *error = "created-role Lua state unavailable";
        return Outcome::kDispatchFailed;
    }
    g_inline_state = state;
    const Outcome outcome = pump_with_dispatch(&inline_dispatch, error);
    g_inline_state = nullptr;
    return outcome;
}

}  // namespace nevergone::created_role_transition
