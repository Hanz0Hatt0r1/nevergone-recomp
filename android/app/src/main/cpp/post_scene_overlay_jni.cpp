#include <GLES2/gl2.h>
#include <jni.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <string>

#include "character_name_compositor.h"
#include "character_name_state.h"
#include "controls_function_state_runtime.h"
#include "game_scene_action_compositor.h"
#include "game_scene_action_surface_input.h"
#include "game_scene_direct_renderer.h"
#include "game_scene_player_joystick_input.h"
#include "offline_startup_flow.h"
#include "scene_actions_gles_compositor.h"
#include "server_selection_compositor.h"
#include "single_select_hero_confirm_compositor.h"
#include "startup_contract.h"
#include "tutorial_attack_live_gate.h"
#include "tutorial_dialogue_runtime.h"
#include "tutorial_lesson_overlay.h"
#include "tutorial_loading_compositor.h"
#include "tutorial_scene_bootstrap.h"
#include "tutorial_session_state.h"

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSplashLayers(JNIEnv*, jclass);
extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnTouch(
    JNIEnv*, jclass, jint, jint, jfloat, jfloat);

namespace {

std::uint64_t g_last_character_name_asset_attempt_generation =
    std::numeric_limits<std::uint64_t>::max();
std::uint64_t g_last_tutorial_loading_asset_attempt =
    std::numeric_limits<std::uint64_t>::max();
std::uint64_t g_last_tutorial_lesson_asset_generation =
    std::numeric_limits<std::uint64_t>::max();
std::uint64_t g_last_tutorial_dialogue_asset_generation =
    std::numeric_limits<std::uint64_t>::max();
std::uint64_t g_last_action_asset_attempt =
    std::numeric_limits<std::uint64_t>::max();
std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};

bool invoke_files_dir_loader(JNIEnv* env, const char* class_name) {
    if (env == nullptr || class_name == nullptr) return false;
    const std::string& files_dir = nevergone::startup::config().files_dir;
    if (files_dir.empty()) return false;

    jclass loader = env->FindClass(class_name);
    if (loader == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    jmethodID reload = env->GetStaticMethodID(
        loader,
        "reloadFromFilesDir",
        "(Ljava/lang/String;)Z");
    if (reload == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(loader);
        return false;
    }

    bool loaded = false;
    jstring path = env->NewStringUTF(files_dir.c_str());
    if (path != nullptr) {
        loaded = env->CallStaticBooleanMethod(loader, reload, path) == JNI_TRUE;
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            loaded = false;
        }
        env->DeleteLocalRef(path);
    }
    env->DeleteLocalRef(loader);
    return loaded;
}

void maybe_stage_character_name_assets(JNIEnv* env) {
    if (env == nullptr) return;
    const auto state = nevergone::character_name_state::snapshot();
    if (!state.active ||
            state.generation == g_last_character_name_asset_attempt_generation) {
        return;
    }
    g_last_character_name_asset_attempt_generation = state.generation;
    (void)invoke_files_dir_loader(env, "org/nevergone/recomp/CharacterNameAssetLoader");
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
    (void)invoke_files_dir_loader(env, "org/nevergone/recomp/TutorialLoadingAssetLoader");
}

void maybe_stage_tutorial_lesson_assets(JNIEnv* env) {
    if (env == nullptr) return;
    const auto session = nevergone::tutorial_session_state::snapshot();
    if (session.phase != nevergone::tutorial_session_state::Phase::kLessonOverlay) {
        g_last_tutorial_lesson_asset_generation =
            std::numeric_limits<std::uint64_t>::max();
        return;
    }
    if (session.generation == g_last_tutorial_lesson_asset_generation) return;
    g_last_tutorial_lesson_asset_generation = session.generation;
    (void)invoke_files_dir_loader(
        env, "org/nevergone/recomp/TutorialLessonOverlayAssetLoader");
}

void maybe_stage_tutorial_dialogue_assets(JNIEnv* env) {
    if (env == nullptr) return;
    const auto session = nevergone::tutorial_session_state::snapshot();
    auto dialogue = nevergone::tutorial_dialogue_runtime::snapshot();

    // Keep the pure tutorial_session_state host-testable. The Android frame
    // boundary observes the exact LessonOverlay -> Dialogue transition and
    // starts the original firstDlgCallBack equivalent here instead.
    if (session.phase == nevergone::tutorial_session_state::Phase::kDialogue &&
            dialogue.stage == nevergone::tutorial_dialogue_runtime::Stage::kInactive) {
        if (!nevergone::tutorial_dialogue_runtime::begin_first_dialogue()) return;
        dialogue = nevergone::tutorial_dialogue_runtime::snapshot();
    }

    // A new loading generation must not inherit a stale dialogue/interlude.
    if (session.phase == nevergone::tutorial_session_state::Phase::kLoading) {
        if (dialogue.stage != nevergone::tutorial_dialogue_runtime::Stage::kInactive) {
            nevergone::tutorial_dialogue_runtime::reset();
        }
        g_last_tutorial_dialogue_asset_generation =
            std::numeric_limits<std::uint64_t>::max();
        return;
    }

    if (session.phase != nevergone::tutorial_session_state::Phase::kDialogue ||
            dialogue.stage != nevergone::tutorial_dialogue_runtime::Stage::kFirstDialogue) {
        g_last_tutorial_dialogue_asset_generation =
            std::numeric_limits<std::uint64_t>::max();
        return;
    }
    if (dialogue.generation == g_last_tutorial_dialogue_asset_generation) return;
    g_last_tutorial_dialogue_asset_generation = dialogue.generation;
    (void)invoke_files_dir_loader(env, "org/nevergone/recomp/TutorialDialogueAssetLoader");
}

void maybe_stage_action_assets(JNIEnv* env) {
    if (env == nullptr) return;
    const auto state = nevergone::offline_startup_flow::snapshot();
    if (state.route != nevergone::offline_startup_flow::Route::kTutorialLoading &&
            state.route != nevergone::offline_startup_flow::Route::kGameLevels) {
        g_last_action_asset_attempt = std::numeric_limits<std::uint64_t>::max();
        return;
    }
    if (state.fresh_role_local_enter_count == g_last_action_asset_attempt) return;
    g_last_action_asset_attempt = state.fresh_role_local_enter_count;
    (void)invoke_files_dir_loader(env, "org/nevergone/recomp/GameSceneActionAssetLoader");
}

bool gameplay_controls_allowed() {
    return nevergone::offline_startup_flow::snapshot().route ==
               nevergone::offline_startup_flow::Route::kGameLevels &&
           !nevergone::tutorial_session_state::modal_input_blocked();
}

void wrapped_server_surface_created(JNIEnv*, jclass) {
    nevergone::game_scene_direct_renderer::on_surface_created();
    nevergone::server_selection_compositor::on_surface_created();
    nevergone::character_name_compositor::on_surface_created();
    nevergone::tutorial_loading_compositor::on_surface_created();
    nevergone::tutorial_lesson_overlay::on_surface_created();
    nevergone::tutorial_dialogue_runtime::on_surface_created();
    nevergone::tutorial_attack_live_gate::live_reset();
    nevergone::scene_actions_gles_compositor::on_surface_created();
    nevergone::game_scene_action_compositor::on_surface_created();
    nevergone::game_scene_action_surface_input::cancel_all_captures();
    nevergone::controls_function_state_runtime::reset();
    g_last_action_asset_attempt = std::numeric_limits<std::uint64_t>::max();
    g_last_tutorial_lesson_asset_generation = std::numeric_limits<std::uint64_t>::max();
    g_last_tutorial_dialogue_asset_generation = std::numeric_limits<std::uint64_t>::max();
    while (glGetError() != GL_NO_ERROR) {
    }
}

void wrapped_server_draw(JNIEnv* env, jclass) {
    nevergone::tutorial_scene_bootstrap::tick();

    bool game_scene_drawn = false;
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] > 0 && viewport[3] > 0) {
        g_surface_width.store(viewport[2], std::memory_order_relaxed);
        g_surface_height.store(viewport[3], std::memory_order_relaxed);
        nevergone::game_scene_direct_renderer::on_surface_changed(viewport[2], viewport[3]);
        nevergone::tutorial_loading_compositor::on_surface_changed(viewport[2], viewport[3]);
        nevergone::tutorial_lesson_overlay::on_surface_changed(viewport[2], viewport[3]);
        nevergone::tutorial_dialogue_runtime::on_surface_changed(viewport[2], viewport[3]);
        nevergone::scene_actions_gles_compositor::on_surface_changed(viewport[2], viewport[3]);
        nevergone::game_scene_action_compositor::on_surface_changed(viewport[2], viewport[3]);
        game_scene_drawn = nevergone::game_scene_direct_renderer::draw();
    }

    nevergone::server_selection_compositor::draw();
    maybe_stage_character_name_assets(env);

    if (viewport[2] > 0 && viewport[3] > 0) {
        nevergone::character_name_compositor::draw(viewport[2], viewport[3]);
    }

    maybe_stage_action_assets(env);

    const bool controls_allowed = gameplay_controls_allowed();
    if (controls_allowed) {
        (void)nevergone::controls_function_state_runtime::tick();
    } else {
        nevergone::controls_function_state_runtime::reset();
        nevergone::game_scene_action_surface_input::cancel_all_captures();
    }

    (void)nevergone::scene_actions_gles_compositor::draw();
    if (controls_allowed) {
        (void)nevergone::game_scene_action_compositor::draw();
    }

    // Runs every GL frame but remains fail-closed until the separately
    // reviewed PLAYERFACEDIR, scene-limit, and enemy-record mapping providers
    // are installed. No default enemy geometry or HP is introduced here.
    (void)nevergone::tutorial_attack_live_gate::live_tick();

    nevergone::tutorial_scene_bootstrap::observe_scene_draw(game_scene_drawn);

    maybe_stage_tutorial_lesson_assets(env);
    (void)nevergone::tutorial_lesson_overlay::draw();

    maybe_stage_tutorial_dialogue_assets(env);
    nevergone::tutorial_dialogue_runtime::tick();
    const auto dialogue_after_tick = nevergone::tutorial_dialogue_runtime::snapshot();
    if (dialogue_after_tick.stage ==
                nevergone::tutorial_dialogue_runtime::Stage::kLessonInterlude &&
            nevergone::tutorial_session_state::snapshot().phase ==
                nevergone::tutorial_session_state::Phase::kDialogue) {
        // Original dialogEndCallBack(1) immediately tail-calls showDialogue().
        // That function exposes the lesson controls; dialogue 9 is scheduled
        // much later from lesson step 12 after enemyNullCount()==0. Do not keep
        // the entire lesson interlude trapped behind the modal Dialogue phase.
        (void)nevergone::tutorial_session_state::on_dialogue_complete();
    }
    (void)nevergone::tutorial_dialogue_runtime::draw();

    maybe_stage_tutorial_loading_assets(env);
    const auto loading_route = nevergone::offline_startup_flow::snapshot();
    nevergone::tutorial_loading_compositor::draw();
    if (loading_route.route == nevergone::offline_startup_flow::Route::kTutorialLoading) {
        nevergone::tutorial_scene_bootstrap::on_loading_frame_presented();
    }
    nevergone::game_scene_player_joystick_input::record_frame_sample(controls_allowed);
}

void wrapped_native_touch(
        JNIEnv* env,
        jclass clazz,
        jint action,
        jint pointer_id,
        jfloat x,
        jfloat y) {
    if (gameplay_controls_allowed()) {
        nevergone::game_scene_player_joystick_input::record_input_event(
            static_cast<int>(action), static_cast<int>(pointer_id),
            static_cast<float>(x), static_cast<float>(y));
    }
    const auto tutorial = nevergone::tutorial_session_state::snapshot();
    if (tutorial.phase == nevergone::tutorial_session_state::Phase::kLessonOverlay) {
        (void)nevergone::tutorial_lesson_overlay::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y));
        nevergone::game_scene_action_surface_input::cancel_all_captures();
        return;
    }
    if (tutorial.phase == nevergone::tutorial_session_state::Phase::kLoading ||
            tutorial.phase == nevergone::tutorial_session_state::Phase::kDialogue) {
        // Guide dialogue 8 is auto_play=true. Only the actual dialogue remains
        // modal; its callback immediately hands control to the lesson runtime.
        nevergone::game_scene_action_surface_input::cancel_all_captures();
        return;
    }

    if (gameplay_controls_allowed()) {
        const int width = g_surface_width.load(std::memory_order_relaxed);
        const int height = g_surface_height.load(std::memory_order_relaxed);
        if (nevergone::game_scene_action_surface_input::on_touch(
                static_cast<int>(action),
                static_cast<int>(pointer_id),
                static_cast<float>(x),
                static_cast<float>(y),
                width,
                height)) {
            return;
        }
    }

    Java_org_nevergone_recomp_GameSurfaceView_nativeOnTouch(
        env, clazz, action, pointer_id, x, y);
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
        {const_cast<char*>("nativeOnServerSelectionSurfaceCreated"), const_cast<char*>("()V"), reinterpret_cast<void*>(&wrapped_server_surface_created)},
        {const_cast<char*>("nativeDrawServerSelectionLayer"), const_cast<char*>("()V"), reinterpret_cast<void*>(&wrapped_server_draw)},
        {const_cast<char*>("nativeDrawSplashLayers"), const_cast<char*>("()V"), reinterpret_cast<void*>(&draw_post_scene_layers)},
        {const_cast<char*>("nativeOnTouch"), const_cast<char*>("(IIFF)V"), reinterpret_cast<void*>(&wrapped_native_touch)},
    };

    const jint result = env->RegisterNatives(
        clazz, methods, static_cast<jint>(sizeof(methods) / sizeof(methods[0])));
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
