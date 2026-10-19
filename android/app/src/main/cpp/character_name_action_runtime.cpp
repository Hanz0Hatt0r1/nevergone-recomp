#include "character_name_action_executor.h"

#include "character_name_state.h"
#include "character_random_name.h"
#include "login_lua_session.h"
#include "offline_startup_flow.h"
#include "startup_contract.h"
#include "tutorial_scene_bootstrap.h"

namespace nevergone::character_name_action_executor {
namespace {

bool production_create_dispatch() {
    // Startup diagnostics may have run before the original assets were
    // imported. Retry the persistent script session at the point where the
    // staged request is actually dispatched.
    if (!login_lua_session::ensure_started()) return false;
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

    // Original ManagementLayer::GoToGameLayer's fresh-role branch passes the
    // bare "cp_lesson.glData" to GameScene::LoadGameLevelsWithFile(), then
    // calls switchScene(). Load the same tutorial GameLevels model before
    // exposing LoadingLayer; do not use the pvp_scene preservation preload for
    // this evidence-backed path.
    if (!tutorial_scene_bootstrap::prepare(startup::config().files_dir)) {
        if (error != nullptr) {
            const auto state = tutorial_scene_bootstrap::snapshot();
            *error = std::string("offline tutorial scene bootstrap stopped at ") +
                tutorial_scene_bootstrap::stage_name(state.stage);
        }
        return;
    }

    if (!offline_startup_flow::on_fresh_role_local_enter()) {
        if (error != nullptr) {
            *error = "offline fresh-role route changed before tutorial LoadingLayer";
        }
        return;
    }

    // GameScene::switchSceneFadeOut() creates LoadingLayer and its recovered
    // 0.5 second callback to switchNextScene(). Arm the clean-room equivalent
    // only after the route itself is tutorial-loading.
    if (!tutorial_scene_bootstrap::begin_loading()) {
        if (error != nullptr) {
            const auto state = tutorial_scene_bootstrap::snapshot();
            *error = std::string("offline tutorial loading arm stopped at ") +
                tutorial_scene_bootstrap::stage_name(state.stage);
        }
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
