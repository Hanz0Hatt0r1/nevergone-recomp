#include <jni.h>

#include <string>

#include "scene_action_pair_document_store.h"
#include "startup_contract.h"

namespace {

std::string from_jstring(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SceneActionPairRuntimeLoader_nativeLoadSlot(
        JNIEnv* env,
        jclass,
        jint variant,
        jint slot,
        jstring relative_path) {
    if (variant < 1 || variant > 4 || (slot != 0 && slot != 1)) return JNI_FALSE;
    const std::string path = from_jstring(env, relative_path);
    if (path.empty()) return JNI_FALSE;
    const auto target = slot == 0
            ? nevergone::scene_action_pair_document_store::Slot::kBorn
            : nevergone::scene_action_pair_document_store::Slot::kIdle;
    const bool loaded = nevergone::scene_action_pair_document_store::load_from_assets(
            target,
            variant,
            nevergone::startup::config().files_dir,
            path);
    return loaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SceneActionPairRuntimeLoader_nativePairReady(
        JNIEnv*, jclass, jint variant) {
    const auto state = nevergone::scene_action_pair_document_store::snapshot();
    return state.pair_ready && state.variant == variant ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_SceneActionPairRuntimeLoader_nativeReset(
        JNIEnv*, jclass) {
    nevergone::scene_action_pair_document_store::reset();
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_SceneActionPairRuntimeLoader_nativeStatusReport(
        JNIEnv* env, jclass) {
    const std::string report = nevergone::scene_action_pair_document_store::status_report();
    return env->NewStringUTF(report.c_str());
}
