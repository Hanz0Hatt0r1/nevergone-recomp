#include <GLES2/gl2.h>
#include <jni.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "character_name_compositor.h"
#include "character_name_state.h"
#include "game_levels_runtime_state.h"
#include "game_scene_direct_asset_frame_gate.h"
#include "game_scene_direct_asset_requests.h"
#include "game_scene_direct_asset_store.h"
#include "render_bridge.h"
#include "server_selection_compositor.h"
#include "single_select_hero_confirm_compositor.h"
#include "startup_contract.h"

namespace nevergone::splash_layer_renderer {
void clear_frames();
void draw();
}

namespace {

std::uint64_t g_last_character_name_asset_attempt_generation =
    std::numeric_limits<std::uint64_t>::max();
nevergone::game_scene_direct_asset_frame_gate::State
    g_game_scene_direct_asset_gate;

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

void maybe_stage_game_scene_direct_assets(JNIEnv* env) {
    if (env == nullptr) return;

    const auto queue =
        nevergone::game_levels_runtime_state::current_scene_render_queue();
    const auto store = nevergone::game_scene_direct_asset_store::snapshot();
    if (!queue.has_value()) {
        const auto decision = nevergone::game_scene_direct_asset_frame_gate::evaluate(
            std::nullopt,
            store.active_revision,
            &g_game_scene_direct_asset_gate);
        if (decision.action ==
            nevergone::game_scene_direct_asset_frame_gate::Action::kClearStore) {
            nevergone::game_scene_direct_asset_store::clear();
        }
        return;
    }

    const auto requests = nevergone::game_scene_direct_asset_requests::build(*queue);
    if (store.active_revision == requests.revision) return;

    const std::string& files_dir = nevergone::startup::config().files_dir;
    if (files_dir.empty()) return;

    const auto decision = nevergone::game_scene_direct_asset_frame_gate::evaluate(
        std::optional<std::uint64_t>(requests.revision),
        store.active_revision,
        &g_game_scene_direct_asset_gate);
    if (decision.action !=
        nevergone::game_scene_direct_asset_frame_gate::Action::kStage) {
        return;
    }

    jclass stager = env->FindClass(
        "org/nevergone/recomp/GameSceneDirectAssetStager");
    if (stager == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }
    jmethodID stage = env->GetStaticMethodID(
        stager,
        "stageFromFilesDir",
        "(Ljava/lang/String;)Z");
    if (stage == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(stager);
        return;
    }

    jstring path = env->NewStringUTF(files_dir.c_str());
    if (path != nullptr) {
        (void)env->CallStaticBooleanMethod(stager, stage, path);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(path);
    }
    env->DeleteLocalRef(stager);
}

void wrapped_on_draw_frame(JNIEnv* env, jclass) {
    nevergone::render::on_draw_frame();
    maybe_stage_game_scene_direct_assets(env);
}

void wrapped_clear_splash_frames(JNIEnv*, jclass) {
    nevergone::splash_layer_renderer::clear_frames();
    nevergone::game_scene_direct_asset_store::clear();
    nevergone::game_scene_direct_asset_frame_gate::invalidate(
        &g_game_scene_direct_asset_gate);
}

void wrapped_server_surface_created(JNIEnv*, jclass) {
    nevergone::server_selection_compositor::on_surface_created();
    nevergone::character_name_compositor::on_surface_created();
    while (glGetError() != GL_NO_ERROR) {
    }
}

void wrapped_server_draw(JNIEnv* env, jclass) {
    nevergone::server_selection_compositor::draw();
    maybe_stage_character_name_assets(env);

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] > 0 && viewport[3] > 0) {
        nevergone::character_name_compositor::draw(viewport[2], viewport[3]);
    }
}

void draw_post_scene_layers(JNIEnv*, jclass) {
    nevergone::splash_layer_renderer::draw();
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
            const_cast<char*>("nativeOnDrawFrame"),
            const_cast<char*>("()V"),
            reinterpret_cast<void*>(&wrapped_on_draw_frame),
        },
        {
            const_cast<char*>("nativeClearSplashFrames"),
            const_cast<char*>("()V"),
            reinterpret_cast<void*>(&wrapped_clear_splash_frames),
        },
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
