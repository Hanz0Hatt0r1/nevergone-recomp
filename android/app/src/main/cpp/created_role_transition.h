#pragma once

#include <cstdint>
#include <string>

#include "login_callback_payload.h"

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

// Stages the role returned by cpp_OnCreateTheRole. The actual EnterGameWithCid
// dispatch is deliberately deferred outside the Lua callback stack so the
// persistent session never has to re-lock itself from a nested callback.
bool stage(const login_callback_payload::RoleEntry& role);

// A failed automatic attempt leaves the created role pending but not due, so a
// render loop cannot hammer the Lua/session boundary every frame. Diagnostics
// or a later recovery path may explicitly arm one retry.
bool request_retry();

Outcome pump_with_dispatch(DispatchFn dispatch, std::string* error = nullptr);
Outcome pump(std::string* error = nullptr);

void reset();
Snapshot snapshot();
const char* outcome_name(Outcome outcome);
std::string status_report();

}  // namespace nevergone::created_role_transition
