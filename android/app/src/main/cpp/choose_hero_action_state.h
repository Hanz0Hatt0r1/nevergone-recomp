#pragma once

#include <cstdint>
#include <string>

namespace nevergone::choose_hero_action_state {

enum class Action : int {
    kNone = 0,
    kOpenCreateHeroScreen = 1,
    kLeaveToSelectHero = 2,
    kStartSelectedHero = 3,
    kSubmitCreateRole = 4,
    kRandomizeRoleName = 5,
    kDetachImeAndRestoreSaveList = 6,
    kAttachIme = 7,
    kConfirmDeleteSelectedHero = 8,
};

struct Request {
    Action action = Action::kNone;
    std::uint64_t scene_generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t selected_hero_id = 0;
    std::int32_t create_role_parameter = 0;
    std::string role_name;
};

struct Snapshot {
    std::uint64_t scene_generation = 0;
    std::uint64_t dispatch_count = 0;
    std::uint64_t consume_count = 0;
    bool pending = false;
    Request latest;
};

bool action_from_tag(int tag, Action* output);
const char* action_name(Action action);

// Records the exact shipped OnCreateback tag plus the runtime context needed by
// later executors. This module intentionally does not perform ManagementLayer,
// MEPlayer, Lua, IME, random-name, or delete side effects itself.
bool dispatch(
    int tag,
    std::uint64_t scene_generation,
    std::uint32_t selected_hero_id,
    std::int32_t create_role_parameter,
    std::string role_name = {});

// Returns the pending request without clearing it.
bool peek(Request* output);

// Clears a pending request only when the caller actually accepts it. This lets
// future executors keep a request pending on recoverable runtime failures.
bool consume(Request* output);

void reset(std::uint64_t scene_generation = 0);
Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::choose_hero_action_state
