#pragma once

#include <cstdint>
#include <string>

namespace nevergone::offline_startup_flow {

enum class Route : int {
    kInactive = 0,
    kRoleProbePending = 1,
    kOpeningDialogue = 2,
    kChooseRole = 3,
    kGameLevels = 4,
};

struct Snapshot {
    Route route = Route::kInactive;
    std::uint64_t scene_generation = 0;
    bool standalone_hero_presence_known = false;
    bool has_standalone_heroes = false;
    std::uint64_t auto_login_success_count = 0;
    std::uint64_t route_resolution_count = 0;
    std::uint64_t fresh_role_local_enter_count = 0;
};

void reset();

// Supplies the recovered standalone hero presence result. If the offline
// auto-login compatibility callback already arrived, this resolves the
// original ReadIcloud branch immediately.
void set_standalone_hero_presence(bool has_standalone_heroes);

// Clean-room compatibility replacement for the obsolete Android SDK's
// callback code 0. This intentionally models only the verified local success
// path: OnLogin -> OnSelectCharacter -> ReadIcloud.
void on_auto_login_compat_success(std::uint64_t scene_generation);

// Project-owned preservation handoff for the serverless fresh-role route.
// Call only after the recovered CreateCharacter Lua dispatch has succeeded
// and the imported GameLevels data has produced a live render queue. This does
// not claim that an original cpp_OnCreateTheRole callback was observed.
bool on_fresh_role_local_enter();

Snapshot snapshot();
const char* route_name(Route route);
std::string status_report();

}  // namespace nevergone::offline_startup_flow
