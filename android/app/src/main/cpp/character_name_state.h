#pragma once

#include <cstdint>
#include <string>

#include "role_creation_validation.h"

namespace nevergone::character_name_state {

constexpr std::size_t kMaxCharacterNameBytes = 18;

enum class Action : int {
    kNone = 0,
    kSubmit = 1,
    kClose = 2,
    kRandomize = 3,
};

struct Snapshot {
    bool active = false;
    std::uint64_t generation = 0;
    std::int64_t career = 0;
    std::string role_name;
    std::uint64_t action_count = 0;
    std::uint64_t submit_count = 0;
    std::uint64_t close_count = 0;
    std::uint64_t completion_count = 0;
    std::uint64_t randomize_count = 0;
    bool randomize_pending = false;
    role_creation_validation::Snapshot last_validation;
};

// Mirrors CharacterNameLayer::CretaUI(int): the argument is stored unchanged
// as the career later passed to LUA_LOGIN::CreateTheRole(name, career). The
// shipped UI immediately dispatches its random-name control after creation.
void begin(std::int64_t career);
void reset();

// CreateTheRoleSuccessful removes the CharacterName layer as a server-success
// side effect, not as the user pressing the Cancel/tag-2 control. Keep that
// completion distinct from close_count/action_count for diagnostics.
bool complete_creation();

bool set_role_name(std::string value);

bool action_from_tag(int tag, Action* output);
const char* action_name(Action action);

// Recovered CharacterNameLayer callback tags:
//   1 -> validate and submit the current name/career
//   2 -> close the layer
//   3 -> randomize the edit-box name
// Successful tag-1 submission stages the existing RoleSelection create request
// but does not consume/dispatch Lua by itself.
bool dispatch_tag(int tag);

bool randomize_pending();
bool take_randomize_request(std::int64_t* career);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::character_name_state
