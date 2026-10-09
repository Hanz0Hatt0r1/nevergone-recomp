#include <GLES2/gl2.h>
#include <jni.h>

#include <cstdint>
#include <limits>
#include <string>

#include "character_name_assets.h"
#include "character_name_compositor.h"
#include "character_name_state.h"
#include "server_selection_compositor.h"
#include "startup_contract.h"

namespace {

std::uint64_t g_last_asset_attempt_generation =
    std::numeric_limits<std::uint64_t>::max();

void maybe_stage_character_name_assets(JNIEnv* env) {
    if (env == nullptr) return;
    const auto state = nevergone::character_name_state::snapshot();
    if (!state.active || state.generation == g_last_asset_attempt_generation) return;
    g_last_asset_attempt_generation = state.generation;

    const std::string& files_dir = nevergone::startup::config().files_dir;
    if (files_dir.empty()) return;

    jclass loader = env->FindClass("org/nevergone/recomp/CharacterNameAssetLoader");
    if (loader == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }
    jmethodID reload = env->GetStaticMethodID(
        loader,
        "reloadFromFilesDir",
        "(Ljava/lang/String;)Z");
    if (reload == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(loader);
        return;
    }
    jstring path = env->NewStringUTF(files_dir.c_str());
    if (path != nullptr) {
        (void)env->CallStaticBooleanMethod(loader, reload, path);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(path);
    }
    env->DeleteLocalRef(loader);
}

void JNICALL wrapped_server_surface_created(JNIEnv*, jclass) {
    nevergone::server_selection_compositor::on_surface_created();
    nevergone::character_name_compositor::on_surface_created();
    // A recreated EGL context may invalidate numeric program/texture names
    // retained by a previous context. Both compositors rebuild explicitly;
    // discard any stale-context GL error before lazy texture uploads inspect
    // glGetError() on their first active frame.
    while (glGetError() != GL_NO_ERROR) {
    }
}

void JNICALL wrapped_server_draw(JNIEnv* env, jclass) {
    // Preserve the existing server-selection layer exactly, then append the
    // fresh-role modal when CharacterName state is active.
    nevergone::server_selection_compositor::draw();
    maybe_stage_character_name_assets(env);

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] > 0 && viewport[3] > 0) {
        nevergone::character_name_compositor::draw(viewport[2], viewport[3]);
    }
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    if (vm == nullptr) return JNI_ERR;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
            env == nullptr) {
        return JNI_ERR;
    }

    jclass surface = env->FindClass("org/nevergone/recomp/GameSurfaceView");
    if (surface == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return JNI_ERR;
    }
    JNINativeMethod methods[] = {
        {
            const_cast<char*>("nativeOnServerSelectionSurfaceCreated"),
            const_cast<char*>("()V"),
            reinterpret_cast<void*>(&wrapped_server_surface_created),
        },
        {
            const_cast<char*>("nativeDrawServerSelectionLayer"),
            const_cast<char*>("()V"),
            reinterpret_cast<void*>(&wrapped_server_draw),
        },
    };
    const jint result = env->RegisterNatives(
        surface,
        methods,
        static_cast<jint>(sizeof(methods) / sizeof(methods[0])));
    env->DeleteLocalRef(surface);
    return result == JNI_OK ? JNI_VERSION_1_6 : JNI_ERR;
}
