#include <jni.h>

#include <cstddef>
#include <cstdint>

#include "game_scene_action_compositor.h"
#include "game_scene_action_surface_input.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSceneActionAssetLoader_nativeSetBounds(
        JNIEnv*, jclass, jint slot, jint width, jint height) {
    return nevergone::game_scene_action_surface_input::set_sprite_bounds(
        static_cast<int>(slot),
        static_cast<int>(width),
        static_cast<int>(height))
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSceneActionAssetLoader_nativeClearBounds(
        JNIEnv*, jclass) {
    nevergone::game_scene_action_surface_input::clear_sprite_bounds();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSceneActionAssetLoader_nativeUploadSprite(
        JNIEnv* env,
        jclass,
        jint slot,
        jboolean selected,
        jint width,
        jint height,
        jintArray pixels) {
    if (env == nullptr || pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const jsize length = env->GetArrayLength(pixels);
    if (expected == 0u || expected > 16777216u ||
        static_cast<std::size_t>(length) != expected) {
        return JNI_FALSE;
    }

    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::game_scene_action_compositor::upload_sprite(
        static_cast<int>(slot),
        selected == JNI_TRUE,
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSceneActionAssetLoader_nativeClearSprites(
        JNIEnv*, jclass) {
    nevergone::game_scene_action_compositor::clear_sprites();
}
