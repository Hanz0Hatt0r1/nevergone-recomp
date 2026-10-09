#include <jni.h>

#include "single_select_hero_confirm_compositor.h"

namespace nevergone::splash_layer_renderer {
void draw();
}

namespace {

void draw_post_scene_layers(JNIEnv*, jclass) {
    // GameSurfaceView calls nativeDrawSplashLayers immediately after the base
    // SingleSelectHero layer. Preserve the existing splash compositor and use
    // the same frame slot for clean-room scene overlays that must appear above
    // SingleSelectHero without changing the Java ABI.
    nevergone::splash_layer_renderer::draw();
    nevergone::single_select_hero_confirm_compositor::draw();
}

void try_register_post_layer(JNIEnv* env) {
    if (env == nullptr) return;
    jclass clazz = env->FindClass("org/nevergone/recomp/GameSurfaceView");
    if (clazz == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    JNINativeMethod method{};
    method.name = const_cast<char*>("nativeDrawSplashLayers");
    method.signature = const_cast<char*>("()V");
    method.fnPtr = reinterpret_cast<void*>(&draw_post_scene_layers);
    if (env->RegisterNatives(clazz, &method, 1) != JNI_OK && env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(clazz);
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    if (vm == nullptr) return JNI_ERR;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4) != JNI_OK ||
            env == nullptr) {
        return JNI_ERR;
    }
    try_register_post_layer(env);
    return JNI_VERSION_1_4;
}
