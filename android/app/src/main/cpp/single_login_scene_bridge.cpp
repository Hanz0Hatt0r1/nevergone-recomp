#include <jni.h>

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

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeResetRecoveredSceneSequence(JNIEnv*, jclass) {
    nevergone::offline_startup_flow::reset();
    nevergone::tap_to_start_state::reset();
    nevergone::splash_sequence_state::reset();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeBeginRecoveredSceneSequence(JNIEnv*, jclass) {
    nevergone::splash_sequence_state::begin(nevergone::game_clock::tick_count());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSingleLoginActive(JNIEnv*, jclass) {
    advance_offline_startup_flow();
    return nevergone::splash_sequence_state::complete(nevergone::game_clock::tick_count())
        ? JNI_TRUE
        : JNI_FALSE;
}
