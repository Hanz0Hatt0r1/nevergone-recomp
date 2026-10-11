#include "character_name_action_executor.h"

#include "character_name_state.h"
#include "character_random_name.h"
#include "game_levels_enter_transition.h"
#include "login_lua_session.h"
#include "offline_startup_flow.h"
#include "startup_contract.h"

namespace nevergone::character_name_action_executor {
namespace {

bool production_create_dispatch() {
    return login_lua_session::dispatch_pending_role_create_request();
}

bool production_randomize(std::string* error) {
    return character_random_name::fulfill_pending(error);
}

void try_offline_fresh_role_handoff(std::string* error) {
    if (offline_startup_flow::snapshot().route !=
            offline_startup_flow::Route::kOpeningDialogue) {
        return;
    }

    // The original fresh-role path waits for cpp_OnCreateTheRole and then
    // enters the returned CharacterID. The preservation route has no live
    // service callback, but the real-device path proves that the recovered
    // CreateCharacter Lua dispatch itself succeeds. Do not invent a synthetic
    // CharacterID. Instead, allow the already verified imported GameLevels
    // model to become the local continuation, and only retire CharacterName
    // after the live render queue is actually ready.
    game_levels_enter_transition::on_enter_game(startup::config().files_dir);
    const auto enter = game_levels_enter_transition::snapshot();
    if (enter.boundary != game_levels_enter_transition::Boundary::kRuntimeRenderQueueReady) {
        if (error != nullptr) {
            *error = std::string("offline GameLevels handoff stopped at ") +
                game_levels_enter_transition::boundary_name(enter.boundary);
        }
        return;
    }

    if (!offline_startup_flow::on_fresh_role_local_enter()) {
        if (error != nullptr) *error = "offline fresh-role route changed before GameLevels handoff";
        return;
    }

    (void)character_name_state::complete_creation();
    if (error != nullptr) error->clear();
}

}  // namespace

Outcome dispatch_tag(int tag, std::string* error) {
    const Outcome outcome = dispatch_tag_with_callbacks(
        tag,
        &production_create_dispatch,
        &production_randomize,
        error);
    if (outcome == Outcome::kSubmitted) {
        try_offline_fresh_role_handoff(error);
    }
    return outcome;
}

}  // namespace nevergone::character_name_action_executor
