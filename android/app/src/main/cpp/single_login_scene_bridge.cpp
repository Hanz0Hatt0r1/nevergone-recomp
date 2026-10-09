#include <jni.h>

#include "app_delegate_state.h"
#include "choose_hero_action_state.h"
#include "choose_hero_role_selection_state.h"
#include "game_clock.h"
#include "initial_ui_transition.h"
#include "offline_startup_flow.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_state.h"
#include "splash_sequence_state.h"
#include "standalone_hero_save_probe.h"
#include "startup_contract.h"
#include "tap_to_start_state.h"

namespace {

void advance_offline_startup_flow() {
    if (!nevergone::tap_to_start_state::consume_auto_login_request()) return;

    const auto tap_state = nevergone::tap_to_start_state::snapshot();
    const auto hero_slots =
        nevergone::standalone_hero_save_probe::standalone_hero_slots(
            nevergone::startup::config().files_dir);

    nevergone::choose_hero_role_selection_state::configure_slots(
        hero_slots,
        tap_state.scene_generation);
    nevergone::offline_startup_flow::set_standalone_hero_presence(!hero_slots.empty());
    nevergone::offline_startup_flow::on_auto_login_compat_success(
        tap_state.scene_generation);
}

void update_initial_ui_transition() {
    const std::uint64_t tick = nevergone::game_clock::tick_count();
    nevergone::app_delegate_state::on_frame(
        tick,
        nevergone::splash_sequence_state::complete(tick));
    const auto app_state = nevergone::app_delegate_state::snapshot();
    nevergone::initial_ui_transition::sync(
        app_state.phase == nevergone::app_delegate_state::Phase::kInitialUiReady,
        app_state.scene_generation);
}

bool management_login_initialized() {
    return nevergone::initial_ui_transition::snapshot().phase ==
        nevergone::initial_ui_transition::Phase::kManagementLoginInitialized;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeResetRecoveredSceneSequence(JNIEnv*, jclass) {
    nevergone::offline_startup_flow::reset();
    nevergone::single_select_hero_state::reset();
    nevergone::single_select_hero_transition_state::reset();
    nevergone::tap_to_start_state::reset();
    nevergone::splash_sequence_state::reset();
    const std::uint64_t generation = nevergone::splash_sequence_state::generation();
    nevergone::choose_hero_role_selection_state::reset(generation);
    nevergone::choose_hero_action_state::reset(generation);
    nevergone::app_delegate_state::on_surface_ready();
    nevergone::app_delegate_state::on_scene_sequence_reset(generation);
    nevergone::initial_ui_transition::reset(generation);
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeBeginRecoveredSceneSequence(JNIEnv*, jclass) {
    const std::uint64_t tick = nevergone::game_clock::tick_count();
    nevergone::splash_sequence_state::begin(tick);
    const std::uint64_t generation = nevergone::splash_sequence_state::generation();
    nevergone::choose_hero_action_state::reset(generation);
    nevergone::app_delegate_state::on_scene_sequence_begin(generation, tick);
    nevergone::initial_ui_transition::reset(generation);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSingleLoginActive(JNIEnv*, jclass) {
    advance_offline_startup_flow();
    update_initial_ui_transition();
    if (!management_login_initialized()) return JNI_FALSE;
    return nevergone::offline_startup_flow::snapshot().route ==
            nevergone::offline_startup_flow::Route::kInactive
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSingleSelectHeroActive(JNIEnv*, jclass) {
    advance_offline_startup_flow();
    update_initial_ui_transition();
    if (!management_login_initialized()) return JNI_FALSE;

    const bool active = nevergone::offline_startup_flow::snapshot().route ==
        nevergone::offline_startup_flow::Route::kOpeningDialogue;
    if (active) {
        if (!nevergone::single_select_hero_state::snapshot().active) {
            // OpeningDialogue is the no-standalone-hero route, so there is no
            // existing career to disable. This mirrors SingleSelectHero::initUI()
            // selecting career 1 and beginning its initial locked Carousel.
            nevergone::single_select_hero_state::begin(0);
        }
        nevergone::single_select_hero_transition_state::sync(
            nevergone::game_clock::tick_count());
        return JNI_TRUE;
    }

    if (nevergone::single_select_hero_state::snapshot().active) {
        nevergone::single_select_hero_state::reset();
    }
    nevergone::single_select_hero_transition_state::reset();
    return JNI_FALSE;
}
