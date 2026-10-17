#include <jni.h>

#include <cstddef>
#include <cstdint>

#include "game_scene_action_compositor.h"
#include "game_scene_action_surface_input.h"
#include "game_scene_player_joystick_input.h"

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

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSceneControlSettingsLoader_nativeApply(
        JNIEnv*,
        jclass,
        jboolean enabled,
        jfloat joystick_x,
        jfloat joystick_y,
        jfloat button_b_x,
        jfloat button_b_y,
        jfloat unique_skill_x,
        jfloat unique_skill_y) {
    const auto joystick = nevergone::game_scene_action_layout::Point{
        static_cast<float>(joystick_x), static_cast<float>(joystick_y)};
    const auto button_b = nevergone::game_scene_action_layout::Point{
        static_cast<float>(button_b_x), static_cast<float>(button_b_y)};
    const auto unique_skill = nevergone::game_scene_action_layout::Point{
        static_cast<float>(unique_skill_x), static_cast<float>(unique_skill_y)};
    if (!nevergone::game_scene_action_surface_input::apply_saved_control_settings(
            enabled == JNI_TRUE, joystick, button_b, unique_skill)) {
        return JNI_FALSE;
    }
    const auto positions = nevergone::game_scene_action_layout::positions_from_saved_settings(
        1136.0f, enabled == JNI_TRUE, joystick, button_b, unique_skill);
    return nevergone::game_scene_player_joystick_input::set_center(
        positions.joystick.x, positions.joystick.y)
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_org_nevergone_recomp_GameSceneControlSettingsLoader_nativePositions(
        JNIEnv* env, jclass) {
    if (env == nullptr) return nullptr;
    const auto actions = nevergone::game_scene_action_surface_input::snapshot();
    const auto joystick = nevergone::game_scene_player_joystick_input::snapshot();
    const jfloat values[] = {
        joystick.center_x, joystick.center_y,
        actions.positions.buttons[0].x, actions.positions.buttons[0].y,
        actions.positions.buttons[1].x, actions.positions.buttons[1].y,
        actions.positions.buttons[2].x, actions.positions.buttons[2].y,
        actions.positions.unique_skill_tip.x, actions.positions.unique_skill_tip.y,
    };
    jfloatArray output = env->NewFloatArray(static_cast<jsize>(sizeof(values) / sizeof(values[0])));
    if (output != nullptr) {
        env->SetFloatArrayRegion(
            output, 0, static_cast<jsize>(sizeof(values) / sizeof(values[0])), values);
    }
    return output;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSceneControlSettingsLoader_nativeSetEditMode(
        JNIEnv*, jclass, jboolean enabled) {
    nevergone::game_scene_action_surface_input::set_edit_mode(enabled == JNI_TRUE);
}
