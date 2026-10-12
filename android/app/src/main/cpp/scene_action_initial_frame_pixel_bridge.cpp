#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "scene_action_initial_frame_pixel_store.h"

namespace {

std::optional<nevergone::scene_action_pair_document_store::Slot> slot_from_jint(jint slot) {
    if (slot == 0) return nevergone::scene_action_pair_document_store::Slot::kBorn;
    if (slot == 1) return nevergone::scene_action_pair_document_store::Slot::kIdle;
    return std::nullopt;
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
    if (!target.has_value() || env == nullptr || pixels == nullptr ||
            variant < 1 || variant > 4 || mode < 0 || mode > 1 ||
            width <= 0 || height <= 0) {
        return JNI_FALSE;
    }
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (h != 0u && w > std::numeric_limits<std::size_t>::max() / h) return JNI_FALSE;
    const std::size_t expected = w * h;
    if (expected == 0u || expected > nevergone::scene_action_initial_frame_pixel_store::kMaxPixelsPerFrame ||
            expected != static_cast<std::size_t>(length)) {
        return JNI_FALSE;
    }

    std::vector<jint> source(static_cast<std::size_t>(length));
    env->GetIntArrayRegion(pixels, 0, length, source.data());
    if (env->ExceptionCheck()) return JNI_FALSE;
    std::vector<std::uint32_t> argb(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        argb[i] = static_cast<std::uint32_t>(source[i]);
    }

    const bool uploaded = nevergone::scene_action_initial_frame_pixel_store::upload(
            *target,
            variant,
            static_cast<std::uint8_t>(mode),
            width,
            height,
            argb.data(),
            argb.size());
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
