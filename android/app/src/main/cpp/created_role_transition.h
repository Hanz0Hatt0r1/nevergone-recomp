#pragma once

#include <cstdint>
#include <string>

#include "login_callback_payload.h"

struct lua_State;

namespace nevergone::created_role_transition {

enum class Outcome {
    kIdle,
    kDispatchFailed,
    kDispatched,
};

struct Snapshot {
    bool pending = false;
    bool dispatch_due = false;
    login_callback_payload::RoleEntry role;
    std::uint64_t stage_count = 0;
    std::uint64_t dispatch_count = 0;
    std::uint64_t dispatch_failure_count = 0;
    std::string last_error;
};

using DispatchFn = bool (*)(const login_callback_payload::RoleEntry&, std::string* error);

// Stages the role returned by cpp_OnCreateTheRole.
bool stage(const login_callback_payload::RoleEntry& role);

// A failed automatic attempt leaves the created role pending but not due, so
// no outer loop can hammer the Lua boundary. Diagnostics may explicitly arm
// one retry.
bool request_retry();

Outcome pump_with_dispatch(DispatchFn dispatch, std::string* error = nullptr);

// Production CreateTheRoleSuccessful-equivalent. It runs after the callback
// capture mutex has been released but on the same Lua state/call stack as the
// shipped native callback, so it can invoke EnterGameWithCid without touching
// the persistent-session mutex recursively.
Outcome pump_inline(lua_State* state, std::string* error = nullptr);

void reset();
Snapshot snapshot();
const char* outcome_name(Outcome outcome);
std::string status_report();

}  // namespace nevergone::created_role_transition
