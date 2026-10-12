#include <GLES2/gl2.h>
#include <jni.h>

#include <cstdint>
#include <limits>
#include <string>

#include "character_name_compositor.h"
#include "character_name_state.h"
#include "game_scene_direct_renderer.h"
#include "offline_startup_flow.h"
#include "server_selection_compositor.h"
#include "single_select_hero_confirm_compositor.h"
#include "startup_contract.h"
#include "tutorial_loading_compositor.h"

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSplashLayers(JNIEnv*, jclass);

namespace {

std::uint64_t g_last_character_name_asset_attempt_generation =
    std::numeric_limits<std::uint64_t>::max();
std::uint64_t g_last_tutorial_loading_asset_attempt =
    std::numeric_limits<std::uint64_t>::max();

void maybe_stage_character_name_assets(JNIEnv* env) {
    if (env == nullptr) return;
    const auto state = nevergone::character_name_state::snapshot();
    if (!state.active ||
            state.generation == g_last_character_name_asset_attempt_generation) {
        return;
    }
    g_last_character_name_asset_attempt_generation = state.generation;

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

void maybe_stage_tutorial_loading_assets(JNIEnv* env) {
    if (env == nullptr) return;
    const auto state = nevergone::offline_startup_flow::snapshot();
    if (state.route != nevergone::offline_startup_flow::Route::kTutorialLoading) {
        g_last_tutorial_loading_asset_attempt =
            std::numeric_limits<std::uint64_t>::max();
        return;
    }
    if (state.fresh_role_local_enter_count == g_last_tutorial_loading_asset_attempt) {
        return;
    }
    g_last_tutorial_loading_asset_attempt = state.fresh_role_local_enter_count;

    const std::string& files_dir = nevergone::startup::config().files_dir;
    if (files_dir.empty()) return;

    jclass loader = env->FindClass("org/nevergone/recomp/TutorialLoadingAssetLoader");
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

void wrapped_server_surface_created(JNIEnv*, jclass) {
    nevergone::game_scene_direct_renderer::on_surface_created();
    nevergone::server_selection_compositor::on_surface_created();
    nevergone::character_name_compositor::on_surface_created();
    nevergone::tutorial_loading_compositor::on_surface_created();
    while (glGetError() != GL_NO_ERROR) {
    }
}

void wrapped_server_draw(JNIEnv* env, jclass) {
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] > 0 && viewport[3] > 0) {
        nevergone::game_scene_direct_renderer::on_surface_changed(viewport[2], viewport[3]);
        nevergone::tutorial_loading_compositor::on_surface_changed(
            viewport[2], viewport[3]);
        (void)nevergone::game_scene_direct_renderer::draw();
    }

    nevergone::server_selection_compositor::draw();
    maybe_stage_character_name_assets(env);

    if (viewport[2] > 0 && viewport[3] > 0) {
        nevergone::character_name_compositor::draw(viewport[2], viewport[3]);
    }

    // This is intentionally last: the original loading layer owns the whole
    // surface while cp_lesson is being prepared, so no already-built gameplay
    // or CharacterName geometry may leak through behind it.
    maybe_stage_tutorial_loading_assets(env);
    nevergone::tutorial_loading_compositor::draw();
}

void draw_post_scene_layers(JNIEnv* env, jclass clazz) {
    Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSplashLayers(env, clazz);
    nevergone::single_select_hero_confirm_compositor::draw();
}

bool register_frame_layers(JNIEnv* env) {
    if (env == nullptr) return false;
    jclass clazz = env->FindClass("org/nevergone/recomp/GameSurfaceView");
    if (clazz == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
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
        {
            const_cast<char*>("nativeDrawSplashLayers"),
            const_cast<char*>("()V"),
            reinterpret_cast<void*>(&draw_post_scene_layers),
        },
    };

    const jint result = env->RegisterNatives(
        clazz,
        methods,
        static_cast<jint>(sizeof(methods) / sizeof(methods[0])));
    if (result != JNI_OK && env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(clazz);
    return result == JNI_OK;
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    if (vm == nullptr) return JNI_ERR;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
            env == nullptr) {
        return JNI_ERR;
    }
    return register_frame_layers(env) ? JNI_VERSION_1_6 : JNI_ERR;
}
