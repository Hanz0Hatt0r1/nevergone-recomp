#include "created_role_transition.h"

#include "login_lua_session.h"
#include "role_selection_state.h"

namespace nevergone::created_role_transition {
namespace {

bool production_dispatch(
        const login_callback_payload::RoleEntry& role,
        std::string* error) {
    if (!role_selection_state::request_enter_role(role)) {
        if (error != nullptr) *error = "failed to stage created-role enter request";
        return false;
    }
    if (!login_lua_session::ensure_started()) {
        if (error != nullptr) {
            *error = login_lua_session::snapshot().last_error;
            if (error->empty()) *error = "persistent login Lua did not start";
        }
        return false;
    }
    if (!login_lua_session::dispatch_pending_role_enter_request()) {
        if (error != nullptr) {
            *error = login_lua_session::snapshot().last_error;
            if (error->empty()) *error = "EnterGameWithCid dispatch failed";
        }
        return false;
    }
    if (error != nullptr) error->clear();
    return true;
}

}  // namespace

Outcome pump(std::string* error) {
    return pump_with_dispatch(&production_dispatch, error);
}

}  // namespace nevergone::created_role_transition
