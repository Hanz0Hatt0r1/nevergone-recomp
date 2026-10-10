#include <jni.h>

#include <cstdint>
#include <limits>

#include "game_scene_direct_asset_store.h"

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetStore_nativeClear(
        JNIEnv*, jclass) {
    nevergone::game_scene_direct_asset_store::clear();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetStore_nativeBegin(
        JNIEnv*,
        jclass,
        jlong revision,
        jint expected_count) {
    if (revision <= 0 || expected_count < 0) return JNI_FALSE;
    return nevergone::game_scene_direct_asset_store::begin(
            static_cast<std::uint64_t>(revision),
            static_cast<std::size_t>(expected_count))
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetStore_nativeUpload(
        JNIEnv* env,
        jclass,
        jlong revision,
        jint request_index,
        jint sprite_command_index,
        jint width,
        jint height,
        jintArray pixels) {
    if (env == nullptr || revision <= 0 || request_index < 0 ||
        sprite_command_index < 0 || pixels == nullptr) {
        return JNI_FALSE;
    }

    const jsize length = env->GetArrayLength(pixels);
    if (length < 0) return JNI_FALSE;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::game_scene_direct_asset_store::upload(
            static_cast<std::uint64_t>(revision),
            static_cast<std::size_t>(request_index),
            static_cast<std::size_t>(sprite_command_index),
            static_cast<int>(width),
            static_cast<int>(height),
            reinterpret_cast<const std::uint32_t*>(values),
            static_cast<std::size_t>(length));
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetStore_nativeFinish(
        JNIEnv*,
        jclass,
        jlong revision) {
    if (revision <= 0) return JNI_FALSE;
    return nevergone::game_scene_direct_asset_store::finish(
            static_cast<std::uint64_t>(revision))
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetStore_nativeCancel(
        JNIEnv*,
        jclass,
        jlong revision) {
    if (revision <= 0) return;
    nevergone::game_scene_direct_asset_store::cancel(
            static_cast<std::uint64_t>(revision));
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetStore_nativeActiveRevision(
        JNIEnv*, jclass) {
    return static_cast<jlong>(
            nevergone::game_scene_direct_asset_store::snapshot().active_revision);
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetStore_nativeActiveAssetCount(
        JNIEnv*, jclass) {
    const auto count = nevergone::game_scene_direct_asset_store::snapshot().active_asset_count;
    if (count > static_cast<std::size_t>(std::numeric_limits<jint>::max())) return 0;
    return static_cast<jint>(count);
}
