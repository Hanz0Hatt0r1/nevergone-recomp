#include <jni.h>

#include <cstdint>
#include <sstream>
#include <string>

#include "controls_function_state_runtime.h"
#include "game_clock.h"
#include "game_levels_runtime_state.h"
#include "game_scene_action_input.h"
#include "game_scene_direct_renderer.h"
#include "game_scene_player_joystick_input.h"
#include "game_scene_player_motion_state.h"
#include "game_scene_player_spawn_state.h"
#include "meplayer_live_action_state.h"
#include "minimal_runtime_save_runtime.h"
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

void sync_runtime_to_spawn(const nevergone::game_scene_player_spawn_state::Snapshot& spawn) {
    const jint career = current_offline_career();
    if (spawn.status != nevergone::game_scene_player_spawn_state::Status::kReady || career <= 0) {
        nevergone::game_scene_player_motion_state::reset();
        nevergone::meplayer_live_action_state::reset();
        return;
    }

    const std::uint64_t tick = nevergone::game_clock::tick_count();
    const std::uint32_t player_index = static_cast<std::uint32_t>(career);
    (void)nevergone::game_scene_player_motion_state::sync_spawn(
            spawn.generation,
            player_index,
            tick);

    // Persistence cannot choose a route, scene or player. It is consulted only
    // after the normal reconstructed spawn/career path is ready and applies a
    // saved offset only when all three identities match.
    const std::string& files_dir = nevergone::startup::config().files_dir;
    (void)nevergone::minimal_runtime_save_runtime::restore_for_spawn(
            files_dir,
            spawn,
            player_index,
            tick);
    (void)nevergone::minimal_runtime_save_runtime::autosave_if_due(files_dir, tick);

    const auto action_input = nevergone::game_scene_action_input::snapshot();
    (void)nevergone::meplayer_live_action_state::sync_spawn(
            spawn.generation,
            action_input.tutorial_event_count);
}

void observe_live_action_sources() {
    const auto input = nevergone::game_scene_action_input::snapshot();
    (void)nevergone::meplayer_live_action_state::observe_tutorial_event_count(
            input.tutorial_event_count);

    const auto function = nevergone::controls_function_state_runtime::snapshot();
    (void)nevergone::meplayer_live_action_state::observe_base_function_state(
            function.generation,
            function.function_state,
            function.has_pending_base_action,
            function.pending_meplayer_state,
            function.pending_has_additional_guard);
}

std::string runtime_status_report() {
    const auto career = nevergone::player_offline_career::current();
    const auto document = nevergone::player_action_document_store::snapshot();

    std::ostringstream out;
    out << "Player action runtime\n";
    out << "career state: " << (career.ready() ? "ready" : "idle") << "\n";
    out << "career: " << career.career << "\n";
    out << "career source: " << nevergone::player_offline_career::source_name(career.source) << "\n";
    out << "document state: "
        << nevergone::player_action_document_store::status_name(document.status) << "\n";
    out << "document source: "
        << (document.embedded_entry_name.empty() ? "standalone" : "actions-data-bundle") << "\n";
    out << "document load attempts: " << document.load_attempt_count << "\n";
    out << "document primary records: " << document.primary_record_count << "\n";
    out << nevergone::controls_function_state_runtime::status_report();
    out << nevergone::meplayer_live_action_state::status_report();
    out << nevergone::minimal_runtime_save_runtime::status_report();
    return out.str();
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
        sync_runtime_to_spawn(existing);
        return JNI_TRUE;
    }

    const auto scene = nevergone::game_levels_runtime_state::current_scene_instance();
    if (!scene.has_value()) {
        nevergone::game_scene_player_spawn_state::reset();
        nevergone::game_scene_player_motion_state::reset();
        nevergone::meplayer_live_action_state::reset();
        return JNI_FALSE;
    }
    const bool ready = nevergone::game_scene_player_spawn_state::refresh(
            *scene,
            levels.stored_event_port_type,
            renderer.surface_width,
            renderer.surface_height);
    if (ready) {
        sync_runtime_to_spawn(nevergone::game_scene_player_spawn_state::snapshot());
    } else {
        nevergone::game_scene_player_motion_state::reset();
        nevergone::meplayer_live_action_state::reset();
    }
    return ready ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeDesiredPlayerActionState(
        JNIEnv*, jclass) {
    const auto spawn = nevergone::game_scene_player_spawn_state::snapshot();
    if (spawn.status != nevergone::game_scene_player_spawn_state::Status::kReady) return -1;

    const auto input = nevergone::game_scene_action_input::snapshot();
    (void)nevergone::meplayer_live_action_state::sync_spawn(
            spawn.generation, input.tutorial_event_count);
    observe_live_action_sources();
    const auto live = nevergone::meplayer_live_action_state::snapshot();
    return live.ready ? static_cast<jint>(live.desired_action_state) : -1;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeCommitLoadedPlayerActionState(
        JNIEnv*, jclass, jint action_state) {
    if (action_state < 0) return JNI_FALSE;
    return nevergone::meplayer_live_action_state::commit_loaded_action(
            static_cast<std::uint32_t>(action_state)) ? JNI_TRUE : JNI_FALSE;
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

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeLoadPlayerActionDocumentFromBundle(
        JNIEnv* env,
        jclass,
        jstring bundle_relative_path,
        jstring entry_name) {
    const std::string bundle = from_jstring(env, bundle_relative_path);
    const std::string entry = from_jstring(env, entry_name);
    if (bundle.empty() || entry.empty()) return JNI_FALSE;
    const bool loaded = nevergone::player_action_document_store::load_embedded_from_assets(
            nevergone::startup::config().files_dir,
            bundle,
            entry);
    return loaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeResetPlayerActionDocument(
        JNIEnv*, jclass) {
    nevergone::player_action_document_store::reset();
    nevergone::game_scene_player_spawn_state::reset();
    nevergone::game_scene_player_joystick_input::reset();
    nevergone::meplayer_live_action_state::reset();
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_PlayerActionRuntimeLoader_nativeStatusReport(
        JNIEnv* env,
        jclass) {
    const std::string report = runtime_status_report();
    return env->NewStringUTF(report.c_str());
}
