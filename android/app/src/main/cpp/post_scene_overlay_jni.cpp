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
#include "offline_startup_flow.h"
#include "scene_actions_gles_compositor.h"
#include "server_selection_compositor.h"
#include "single_select_hero_confirm_compositor.h"
#include "startup_contract.h"
#include "tutorial_dialogue_overlay.h"
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
    if (session.phase != nevergone::tutorial_session_state::Phase::kDialogue) {
        g_last_tutorial_dialogue_asset_generation =
            std::numeric_limits<std::uint64_t>::max();
        return;
    }
    if (session.generation == g_last_tutorial_dialogue_asset_generation) return;
    g_last_tutorial_dialogue_asset_generation = session.generation;
    (void)invoke_files_dir_loader(
        env, "org/nevergone/recomp/TutorialDialogueAssetLoader");
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
    nevergone::tutorial_dialogue_overlay::on_surface_created();
    nevergone::scene_actions_gles_compositor::on_surface_created();
    nevergone::game_scene_action_compositor::on_surface_created();
    nevergone::game_scene_action_surface_input::cancel_all_captures();
    nevergone::controls_function_state_runtime::reset();
    // A GLES context recreation invalidates action/tutorial textures even when
    // the corresponding fresh-role generation is unchanged. Force restaging.
    g_last_action_asset_attempt = std::numeric_limits<std::uint64_t>::max();
    g_last_tutorial_lesson_asset_generation = std::numeric_limits<std::uint64_t>::max();
    g_last_tutorial_dialogue_asset_generation = std::numeric_limits<std::uint64_t>::max();
    while (glGetError() != GL_NO_ERROR) {
    }
}

void wrapped_server_draw(JNIEnv* env, jclass) {
    // nativeOnDrawFrame() has already advanced the fixed game clock for this
    // frame. Reproduce the shipped LoadingLayer action boundary: after the
    // recovered 0.5 second delay, make switchNextScene eligible. cp_lesson was
    // already resolved and parsed before tutorial-loading was entered, matching
    // ManagementLayer::GoToGameLayer -> LoadGameLevelsWithFile -> switchScene.
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
        nevergone::tutorial_dialogue_overlay::on_surface_changed(viewport[2], viewport[3]);
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

    // LessonLayer::showKeyboardPic() in the shipped tutorial immediately calls
    // Controls::hideControls() and GameSceneUI::hideToolsUI(). Preserve that
    // modal boundary: Loading, LessonOverlay and the post-lesson Dialogue must
    // not tick/draw the ordinary gameplay controls. Interactive is the first
    // phase that unlocks them.
    const bool controls_allowed = gameplay_controls_allowed();
    if (controls_allowed) {
        (void)nevergone::controls_function_state_runtime::tick();
    } else {
        nevergone::controls_function_state_runtime::reset();
        nevergone::game_scene_action_surface_input::cancel_all_captures();
    }

    // SceneActions are world/scene children and remain visible behind tutorial
    // overlays. The A/B/C/S gameplay controls are separate and stay hidden
    // while the tutorial session is modal.
    (void)nevergone::scene_actions_gles_compositor::draw();
    if (controls_allowed) {
        (void)nevergone::game_scene_action_compositor::draw();
    }

    // Once the recovered 0.5 s callback is due, a successful frame from the
    // exact retained cp_lesson queue is the clean-room readiness gate for the
    // original switchNextScene/initGameScene -> tick -> showLesson transition.
    // The rest of this frame still used the old tutorial-loading route, so
    // Controls cannot leak through behind LoadingLayer.
    nevergone::tutorial_scene_bootstrap::observe_scene_draw(game_scene_drawn);

    // The successful handoff above moves the session to LessonOverlay. Stage
    // only user-imported lesson assets, then draw the recovered centered
    // keyboard presentation over the live cp_lesson scene. No timer advances
    // this modal layer: only its Continue callback enters Dialogue.
    maybe_stage_tutorial_lesson_assets(env);
    (void)nevergone::tutorial_lesson_overlay::draw();

    // LessonLayer::menuContinueCallback() schedules firstDlgCallBack(), which
    // opens Guide/guide_text.plist group 8.  Stage and draw that exact imported
    // manual DialogueUI sequence while the session owns its Dialogue phase.
    maybe_stage_tutorial_dialogue_assets(env);
    (void)nevergone::tutorial_dialogue_overlay::draw();

    // LoadingLayer owns the complete surface until the handoff above succeeds.
    maybe_stage_tutorial_loading_assets(env);
    const auto loading_route = nevergone::offline_startup_flow::snapshot();
    nevergone::tutorial_loading_compositor::draw();
    if (loading_route.route == nevergone::offline_startup_flow::Route::kTutorialLoading) {
        nevergone::tutorial_scene_bootstrap::on_loading_frame_presented();
    }
}

void wrapped_native_touch(
        JNIEnv* env,
        jclass clazz,
        jint action,
        jint pointer_id,
        jfloat x,
        jfloat y) {
    const auto tutorial = nevergone::tutorial_session_state::snapshot();
    if (tutorial.phase == nevergone::tutorial_session_state::Phase::kLessonOverlay) {
        // LessonOverlay owns the whole surface. Its Continue target is handled
        // internally; every other touch is swallowed exactly at this modal
        // boundary rather than leaking to joystick or action controls.
        (void)nevergone::tutorial_lesson_overlay::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y));
        nevergone::game_scene_action_surface_input::cancel_all_captures();
        return;
    }
    if (tutorial.phase == nevergone::tutorial_session_state::Phase::kDialogue) {
        // Original DialogueUI::ccTouchBegan accepts the complete surface and
        // ccTouchEnded forwards manual conversations to GameConversation::nextText().
        (void)nevergone::tutorial_dialogue_overlay::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y));
        nevergone::game_scene_action_surface_input::cancel_all_captures();
        return;
    }
    if (tutorial.phase == nevergone::tutorial_session_state::Phase::kLoading) {
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
