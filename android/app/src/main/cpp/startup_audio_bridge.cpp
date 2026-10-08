#include <jni.h>

#include "game_clock.h"
#include "splash_sequence_state.h"
#include "splash_timeline.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSplashSoundDue(JNIEnv*, jclass) {
    const std::uint64_t now_tick = nevergone::game_clock::tick_count();
    if (!nevergone::splash_sequence_state::started() ||
        nevergone::splash_sequence_state::complete(now_tick)) {
        return JNI_FALSE;
    }

    const std::uint64_t elapsed_tick =
        nevergone::splash_sequence_state::elapsed_tick(now_tick);
    const double seconds = static_cast<double>(elapsed_tick) *
        nevergone::game_clock::kFixedStepSeconds;
    return seconds >= nevergone::splash_timeline::kStartupSoundSeconds
        ? JNI_TRUE
        : JNI_FALSE;
}
