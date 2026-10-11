#include <jni.h>

#include <cstdint>
#include <string>

#include "game_levels_runtime_state.h"
#include "player_action_document_store.h"
#include "role_selection_state.h"
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

jint current_offline_career() {
    const auto role = nevergone::role_selection_state::snapshot();
    const std::int64_t career = role.dispatched_career;
    if (!role.enter_dispatch_committed || career < 1 || career > 5) return 0;
    return static_cast<jint>(career);
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeIsOfflineSceneRenderQueueReady(
        JNIEnv*, jclass) {
    return nevergone::game_levels_runtime_state::snapshot().current_scene_render_queue_ready
            ? JNI_TRUE
            : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeCurrentOfflineCareer(
        JNIEnv*, jclass) {
    return current_offline_career();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeLoadPlayerActionDocument(
        JNIEnv* env,
        jclass,
        jstring relative_path) {
    const std::string path = from_jstring(env, relative_path);
    if (path.empty()) return JNI_FALSE;
    const bool loaded = nevergone::player_action_document_store::load_from_assets(
            nevergone::startup::config().files_dir,
            path);
    return loaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeResetPlayerActionDocument(
        JNIEnv*, jclass) {
    nevergone::player_action_document_store::reset();
}
