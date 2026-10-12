#include <jni.h>

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
