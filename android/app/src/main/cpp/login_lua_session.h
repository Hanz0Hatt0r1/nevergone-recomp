#pragma once

#include <cstdint>
#include <string>

namespace nevergone::login_lua_session {

enum class Phase {
    kStopped,
    kStarting,
    kReady,
    kFailed,
};

struct Snapshot {
    Phase phase = Phase::kStopped;
    std::uint64_t generation = 0;
    std::uint64_t start_count = 0;
    std::uint64_t dispatch_count = 0;
    std::uint64_t dispatch_failure_count = 0;
    std::string last_error;
};

bool ensure_started();
void shutdown();

// Pending requests are consumed only after the matching Lua call succeeds;
// failed calls remain pending for diagnostics/retry.
bool dispatch_pending_server_request();
bool dispatch_pending_role_enter_request();
bool dispatch_pending_role_create_request();

Snapshot snapshot();
const char* phase_name(Phase phase);
std::string startup_report();
std::string status_report();

}  // namespace nevergone::login_lua_session
