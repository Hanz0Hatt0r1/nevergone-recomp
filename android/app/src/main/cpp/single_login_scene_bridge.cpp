#include <jni.h>

#include "game_clock.h"
#include "splash_sequence_state.h"

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeResetRecoveredSceneSequence(JNIEnv*, jclass) {
    nevergone::splash_sequence_state::reset();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeBeginRecoveredSceneSequence(JNIEnv*, jclass) {
    nevergone::splash_sequence_state::begin(nevergone::game_clock::tick_count());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSingleLoginActive(JNIEnv*, jclass) {
    return nevergone::splash_sequence_state::complete(nevergone::game_clock::tick_count())
        ? JNI_TRUE
        : JNI_FALSE;
}
