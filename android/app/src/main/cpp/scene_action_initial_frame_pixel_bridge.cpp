#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "scene_action_initial_frame_pixel_store.h"

namespace {

std::optional<nevergone::scene_action_pair_document_store::Slot> slot_from_jint(jint slot) {
    if (slot == 0) return nevergone::scene_action_pair_document_store::Slot::kBorn;
    if (slot == 1) return nevergone::scene_action_pair_document_store::Slot::kIdle;
    return std::nullopt;
}

std::optional<std::vector<std::uint32_t>> copy_argb(
        JNIEnv* env,
        jint width,
        jint height,
        jintArray pixels) {
    if (env == nullptr || pixels == nullptr || width <= 0 || height <= 0) return std::nullopt;
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (h != 0u && w > std::numeric_limits<std::size_t>::max() / h) return std::nullopt;
    const std::size_t expected = w * h;
    if (expected == 0u ||
            expected > nevergone::scene_action_initial_frame_pixel_store::kMaxPixelsPerFrame ||
            expected != static_cast<std::size_t>(length)) {
        return std::nullopt;
    }

    std::vector<jint> source(static_cast<std::size_t>(length));
    env->GetIntArrayRegion(pixels, 0, length, source.data());
    if (env->ExceptionCheck()) return std::nullopt;
    std::vector<std::uint32_t> argb(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        argb[i] = static_cast<std::uint32_t>(source[i]);
    }
    return argb;
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SceneActionInitialFramePixelStager_nativeUpload(
        JNIEnv* env,
        jclass,
        jint slot,
        jint variant,
        jint mode,
        jint width,
        jint height,
        jintArray pixels) {
    const auto target = slot_from_jint(slot);
    if (!target.has_value() || variant < 1 || variant > 4 || mode < 0 || mode > 1) {
        return JNI_FALSE;
    }
    const auto argb = copy_argb(env, width, height, pixels);
    if (!argb.has_value()) return JNI_FALSE;

    const bool uploaded = nevergone::scene_action_initial_frame_pixel_store::upload(
            *target,
            variant,
            static_cast<std::uint8_t>(mode),
            width,
            height,
            argb->data(),
            argb->size());
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SceneActionInitialFramePixelStager_nativeReady(
        JNIEnv*, jclass, jint slot, jint variant) {
    const auto target = slot_from_jint(slot);
    if (!target.has_value()) return JNI_FALSE;
    return nevergone::scene_action_initial_frame_pixel_store::ready(*target, variant)
            ? JNI_TRUE
            : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_SceneActionInitialFramePixelStager_nativeReset(
        JNIEnv*, jclass) {
    nevergone::scene_action_initial_frame_pixel_store::reset();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SceneActionCurrentFramePixelStager_nativeUploadFrame(
        JNIEnv* env,
        jclass,
        jint slot,
        jint variant,
        jint mode,
        jint frame_index,
        jint width,
        jint height,
        jintArray pixels) {
    const auto target = slot_from_jint(slot);
    if (!target.has_value() || variant < 1 || variant > 4 || mode < 0 || mode > 1 ||
            frame_index < 0) {
        return JNI_FALSE;
    }
    const auto argb = copy_argb(env, width, height, pixels);
    if (!argb.has_value()) return JNI_FALSE;

    const bool uploaded = nevergone::scene_action_initial_frame_pixel_store::upload_frame(
            *target,
            variant,
            static_cast<std::uint8_t>(mode),
            static_cast<std::size_t>(frame_index),
            width,
            height,
            argb->data(),
            argb->size());
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SceneActionCurrentFramePixelStager_nativeReadyFrame(
        JNIEnv*, jclass, jint slot, jint variant, jint frame_index) {
    const auto target = slot_from_jint(slot);
    if (!target.has_value() || frame_index < 0) return JNI_FALSE;
    return nevergone::scene_action_initial_frame_pixel_store::ready_frame(
                    *target,
                    variant,
                    static_cast<std::size_t>(frame_index))
            ? JNI_TRUE
            : JNI_FALSE;
}
