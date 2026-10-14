#include <jni.h>

#include <string>

#include "tutorial_scene_bootstrap.h"
#include "tutorial_session_state.h"

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneAssetStagingDiagnostics_nativeTutorialBootstrapStatusReport(
        JNIEnv* env,
        jclass) {
    if (env == nullptr) return nullptr;
    std::string report = nevergone::tutorial_scene_bootstrap::status_report();
    report += nevergone::tutorial_session_state::status_report();
    return env->NewStringUTF(report.c_str());
}
