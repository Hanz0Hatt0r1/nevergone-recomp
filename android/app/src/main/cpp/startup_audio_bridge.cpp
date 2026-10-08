#include <jni.h>

#include "game_clock.h"
#include "splash_timeline.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsSplashSoundDue(JNIEnv*, jclass) {
    const std::uint64_t tick = nevergone::game_clock::tick_count();
    const double seconds = static_cast<double>(tick) * nevergone::game_clock::kFixedStepSeconds;
    const auto sample = nevergone::splash_timeline::sample_tick(tick);
    const bool due = seconds >= nevergone::splash_timeline::kStartupSoundSeconds && !sample.complete;
    return due ? JNI_TRUE : JNI_FALSE;
}
