#include <jni.h>

#include "app_delegate_state.h"
#include "game_clock.h"
#include "offline_startup_flow.h"
#include "splash_sequence_state.h"
#include "standalone_hero_save_probe.h"
#include "startup_contract.h"
#include "tap_to_start_state.h"

namespace {

void advance_offline_startup_flow() {
    if (!nevergone::tap_to_start_state::consume_auto_login_request()) return;

    const auto tap_state = nevergone::tap_to_start_state::snapshot();
    const bool has_standalone_heroes =
        nevergone::standalone_hero_save_probe::has_standalone_hero_save(
            nevergone::startup::config().files_dir);

    nevergone::offline_startup_flow::set_standalone_hero_presence(has_standalone_heroes);
    nevergone::offline_startup_flow::on_auto_login_compat_success(
        tap_state.scene_generation);
}

void update_app_delegate_scene_phase() {
    const std::uint64_t tick = nevergone::game_clock::tick_count();
    nevergone::app_delegate_state::on_frame(
        tick,
        nevergone::splash_sequence_state::complete(tick));
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeResetRecoveredSceneSequence(JNIEnv*, jclass) {
    nevergone::offline_startup_flow::reset();
    nevergone::tap_to_start_state::reset();
    nevergone::splash_sequence_state::reset();
    nevergone::app_delegate_state::on_surface_ready();
    nevergone::app_delegate_state::on_scene_sequence_reset(
        nevergone::splash_sequence_state::generation());
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeBeginRecoveredSceneSequence(JNIEnv*, jclass) {
    const std::uint64_t tick = nevergone::game_clock::tick_count();
    nevergone::splash_sequence_state::begin(tick);
    nevergone::app_delegate_state::on_scene_sequence_begin(
        nevergone::splash_sequence_state::generation(),
        tick);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSingleLoginActive(JNIEnv*, jclass) {
    advance_offline_startup_flow();
    update_app_delegate_scene_phase();
    if (!nevergone::splash_sequence_state::complete(nevergone::game_clock::tick_count())) {
        return JNI_FALSE;
    }
    return nevergone::offline_startup_flow::snapshot().route ==
            nevergone::offline_startup_flow::Route::kInactive
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSingleSelectHeroActive(JNIEnv*, jclass) {
    advance_offline_startup_flow();
    update_app_delegate_scene_phase();
    return nevergone::offline_startup_flow::snapshot().route ==
            nevergone::offline_startup_flow::Route::kOpeningDialogue
        ? JNI_TRUE
        : JNI_FALSE;
}
