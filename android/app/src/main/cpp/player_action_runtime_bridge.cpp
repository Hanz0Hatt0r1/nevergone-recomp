#include <jni.h>

#include <cstdint>
#include <string>

#include "game_clock.h"
#include "game_levels_runtime_state.h"
#include "game_scene_direct_renderer.h"
#include "game_scene_player_joystick_input.h"
#include "game_scene_player_motion_state.h"
#include "game_scene_player_spawn_state.h"
#include "player_action_document_store.h"
#include "player_offline_career.h"
#include "startup_contract.h"

namespace {

std::string from_jstring(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

jint current_offline_career() {
    const auto resolution = nevergone::player_offline_career::current();
    return resolution.ready() ? static_cast<jint>(resolution.career) : 0;
}

void sync_motion_to_spawn(const nevergone::game_scene_player_spawn_state::Snapshot& spawn) {
    const jint career = current_offline_career();
    if (spawn.status != nevergone::game_scene_player_spawn_state::Status::kReady || career <= 0) {
        nevergone::game_scene_player_motion_state::reset();
        return;
    }
    (void)nevergone::game_scene_player_motion_state::sync_spawn(
            spawn.generation,
            static_cast<std::uint32_t>(career),
            nevergone::game_clock::tick_count());
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeIsOfflineSceneRenderQueueReady(
        JNIEnv*, jclass) {
    return nevergone::game_levels_runtime_state::snapshot().current_scene_render_queue_ready
            ? JNI_TRUE
            : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeCurrentOfflineCareer(
        JNIEnv*, jclass) {
    return current_offline_career();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeRefreshPlayerSpawnState(
        JNIEnv*, jclass) {
    const auto levels = nevergone::game_levels_runtime_state::snapshot();
    const auto renderer = nevergone::game_scene_direct_renderer::snapshot();
    const auto existing = nevergone::game_scene_player_spawn_state::snapshot();

    if (levels.current_scene_instance_ready && levels.current_scene_index.has_value() &&
        existing.status == nevergone::game_scene_player_spawn_state::Status::kReady &&
        existing.source_scene_index == *levels.current_scene_index &&
        existing.scene_guid == levels.current_scene_guid &&
        existing.event_port_type == levels.stored_event_port_type &&
        existing.surface_width == renderer.surface_width &&
        existing.surface_height == renderer.surface_height) {
        sync_motion_to_spawn(existing);
        return JNI_TRUE;
    }

    const auto scene = nevergone::game_levels_runtime_state::current_scene_instance();
    if (!scene.has_value()) {
        nevergone::game_scene_player_spawn_state::reset();
        nevergone::game_scene_player_motion_state::reset();
        return JNI_FALSE;
    }
    const bool ready = nevergone::game_scene_player_spawn_state::refresh(
            *scene,
            levels.stored_event_port_type,
            renderer.surface_width,
            renderer.surface_height);
    if (ready) {
        sync_motion_to_spawn(nevergone::game_scene_player_spawn_state::snapshot());
    } else {
        nevergone::game_scene_player_motion_state::reset();
    }
    return ready ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeLoadPlayerActionDocument(
        JNIEnv* env,
        jclass,
        jstring relative_path) {
    const std::string path = from_jstring(env, relative_path);
    if (path.empty()) return JNI_FALSE;
    const bool loaded = nevergone::player_action_document_store::load_from_assets(
            nevergone::startup::config().files_dir,
            path);
    return loaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeResetPlayerActionDocument(
        JNIEnv*, jclass) {
    nevergone::player_action_document_store::reset();
    nevergone::game_scene_player_spawn_state::reset();
    nevergone::game_scene_player_joystick_input::reset();
}
