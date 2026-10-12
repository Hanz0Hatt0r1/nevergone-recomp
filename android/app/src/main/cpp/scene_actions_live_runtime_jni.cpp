#include <jni.h>

#include <cstddef>
#include <limits>
#include <optional>

#include "scene_action_pair_document_store.h"
#include "scene_actions_klhuo_system_inputs.h"
#include "scene_actions_live_runtime.h"
#include "scene_actions_render_runtime.h"

namespace {

std::optional<nevergone::scene_action_pair_document_store::Slot> slot_from_jint(jint slot) {
    if (slot == 0) return nevergone::scene_action_pair_document_store::Slot::kBorn;
    if (slot == 1) return nevergone::scene_action_pair_document_store::Slot::kIdle;
    return std::nullopt;
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_SceneActionLiveRuntime_nativeStartCurrentScene(
        JNIEnv*, jclass, jint variant) {
    const auto result = nevergone::scene_actions_live_runtime::start_current_scene(
            static_cast<int>(variant));
    return static_cast<jint>(result.status);
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_SceneActionLiveRuntime_nativeTick(
        JNIEnv*, jclass, jint slot, jfloat dt_seconds) {
    const auto target = slot_from_jint(slot);
    if (!target.has_value()) {
        return static_cast<jint>(
                nevergone::scene_actions_active_playback::TickStatus::kRejected);
    }
    const auto result = nevergone::scene_actions_live_runtime::tick(
            *target, static_cast<float>(dt_seconds));
    return static_cast<jint>(result.status);
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_SceneActionLiveRuntime_nativeCurrentFrameIndex(
        JNIEnv*, jclass, jint slot) {
    const auto target = slot_from_jint(slot);
    if (!target.has_value()) return -1;
    const auto state = nevergone::scene_actions_live_runtime::snapshot(*target);
    if (!state.has_value() || !state->active || state->playback.done_1a8) return -1;
    const std::size_t frame = state->playback.frame_index_250;
    if (frame > static_cast<std::size_t>(std::numeric_limits<jint>::max())) return -1;
    return static_cast<jint>(frame);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SceneActionLiveRuntime_nativeReadyToSubmit(
        JNIEnv*, jclass, jint slot) {
    const auto target = slot_from_jint(slot);
    if (!target.has_value()) return JNI_FALSE;
    const auto state = nevergone::scene_actions_live_runtime::snapshot(*target);
    if (!state.has_value()) return JNI_FALSE;
    const auto prepared = nevergone::scene_actions_render_runtime::prepare_from_store(
            *state, nevergone::scene_actions_klhuo_system_inputs::initial());
    return prepared.status == nevergone::scene_actions_render_runtime::PrepareStatus::kReady &&
            prepared.frame.has_value()
            ? JNI_TRUE
            : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_SceneActionLiveRuntime_nativeSubmit(
        JNIEnv*, jclass, jint slot) {
    const auto target = slot_from_jint(slot);
    if (!target.has_value()) {
        return static_cast<jint>(
                nevergone::scene_actions_render_submit::SubmitStatus::kRejected);
    }
    const auto result = nevergone::scene_actions_live_runtime::submit_current(*target);
    return static_cast<jint>(result.status);
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_SceneActionLiveRuntime_nativeCurrentVariant(
        JNIEnv*, jclass) {
    return static_cast<jint>(nevergone::scene_actions_live_runtime::variant());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SceneActionLiveRuntime_nativeStarted(
        JNIEnv*, jclass) {
    return nevergone::scene_actions_live_runtime::started() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_SceneActionLiveRuntime_nativeReset(
        JNIEnv*, jclass) {
    nevergone::scene_actions_live_runtime::reset();
}
