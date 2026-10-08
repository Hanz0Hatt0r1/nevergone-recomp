#include <jni.h>

#include "game_clock.h"
#include "splash_timeline.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSingleLoginActive(JNIEnv*, jclass) {
    return nevergone::splash_timeline::sample_tick(
               nevergone::game_clock::tick_count())
                   .complete
        ? JNI_TRUE
        : JNI_FALSE;
}
