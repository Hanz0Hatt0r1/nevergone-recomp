#include <jni.h>

#include "game_scene_direct_sprite_renderer.h"
#include "game_scene_direct_texture_gl.h"

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnGameSceneDirectTexturesSurfaceCreated(
        JNIEnv*,
        jclass) {
    nevergone::game_scene_direct_texture_gl::on_surface_created();
    nevergone::game_scene_direct_sprite_renderer::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearGameSceneDirectTextures(
        JNIEnv*,
        jclass) {
    nevergone::game_scene_direct_texture_gl::clear();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeSyncGameSceneDirectTextures(
        JNIEnv*,
        jclass) {
    return nevergone::game_scene_direct_texture_gl::sync() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawGameSceneDirectSprites(
        JNIEnv*,
        jclass) {
    return static_cast<jint>(nevergone::game_scene_direct_sprite_renderer::draw());
}
