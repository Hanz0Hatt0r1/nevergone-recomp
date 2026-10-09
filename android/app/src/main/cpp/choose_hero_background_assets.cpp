#include <jni.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <utility>

#include "choose_hero_background_assets.h"
#include "choose_hero_background_compositor.h"
#include "choose_hero_black_cloud_compositor.h"
#include "choose_hero_role_focus_compositor.h"
#include "choose_hero_role_item_compositor.h"
#include "choose_hero_thunder_effect_compositor.h"
#include "offline_startup_flow.h"

namespace nevergone::choose_hero_background {
namespace {

std::mutex g_mutex;
std::array<FrameAsset, kFrameCount> g_frames{};
std::uint64_t g_generation = 0;

bool frame_valid(const FrameAsset& frame, int index) {
    const bool design_canvas_ok = index >= kDesignPhaseFrameCount ||
            (frame.source_width == kDesignWidth && frame.source_height == kDesignHeight);
    return frame.width > 0 &&
            frame.height > 0 &&
            frame.source_width > 0 &&
            frame.source_height > 0 &&
            design_canvas_ok &&
            frame.left >= 0 &&
            frame.top >= 0 &&
            frame.left + frame.width <= frame.source_width &&
            frame.top + frame.height <= frame.source_height &&
            frame.pixels.size() ==
                    static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
}

bool range_ready_locked(int first, int end) {
    if (first < 0 || end > kFrameCount || first > end) return false;
    for (int index = first; index < end; ++index) {
        if (!frame_valid(g_frames[static_cast<std::size_t>(index)], index)) return false;
    }
    return true;
}

bool upload_from_java(
        JNIEnv* env,
        jint index,
        jint width,
        jint height,
        jint left,
        jint top,
        jint source_width,
        jint source_height,
        jintArray pixels) {
    if (pixels == nullptr || width <= 0 || height <= 0) return false;
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected) return false;

    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return false;
    const bool uploaded = upload(
            static_cast<int>(index),
            static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(left),
            static_cast<int>(top),
            static_cast<int>(source_width),
            static_cast<int>(source_height),
            reinterpret_cast<const std::uint32_t*>(values),
            expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return uploaded;
}

}  // namespace

void clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (FrameAsset& frame : g_frames) frame = {};
    ++g_generation;
}

bool upload(
        int index,
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (index < 0 || index >= kFrameCount ||
            width <= 0 || height <= 0 || source_width <= 0 || source_height <= 0 ||
            (index < kDesignPhaseFrameCount &&
                    (source_width != kDesignWidth || source_height != kDesignHeight)) ||
            left < 0 || top < 0 ||
            left + width > source_width || top + height > source_height ||
            pixels == nullptr ||
            count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
            count > 16777216u) {
        return false;
    }

    FrameAsset frame;
    frame.width = width;
    frame.height = height;
    frame.left = left;
    frame.top = top;
    frame.source_width = source_width;
    frame.source_height = source_height;
    frame.pixels.assign(pixels, pixels + count);
    if (!frame_valid(frame, index)) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_frames[static_cast<std::size_t>(index)] = std::move(frame);
    ++g_generation;
    return true;
}

bool ready() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return range_ready_locked(0, kBackgroundFrameCount);
}

bool effects_ready() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return range_ready_locked(kLightningFirstIndex, kFrameCount);
}

bool route_active() {
    return offline_startup_flow::snapshot().route == offline_startup_flow::Route::kChooseRole;
}

std::uint64_t generation() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_generation;
}

bool copy_frame(int index, FrameAsset* output) {
    if (output == nullptr || index < 0 || index >= kFrameCount) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    const FrameAsset& frame = g_frames[static_cast<std::size_t>(index)];
    if (!frame_valid(frame, index)) {
        *output = {};
        return false;
    }
    *output = frame;
    return true;
}

}  // namespace nevergone::choose_hero_background

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearChooseHeroBackgroundAssets(
        JNIEnv*, jclass) {
    nevergone::choose_hero_background::clear();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadChooseHeroBackgroundAsset(
        JNIEnv* env,
        jclass,
        jint index,
        jint width,
        jint height,
        jint left,
        jint top,
        jint source_width,
        jint source_height,
        jintArray pixels) {
    return nevergone::choose_hero_background::upload_from_java(
            env,
            index,
            width,
            height,
            left,
            top,
            source_width,
            source_height,
            pixels)
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeChooseHeroBackgroundAssetsReady(
        JNIEnv*, jclass) {
    return nevergone::choose_hero_background::ready() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ChooseHeroEffectStager_nativeUploadEffectAsset(
        JNIEnv* env,
        jclass,
        jint index,
        jint width,
        jint height,
        jint left,
        jint top,
        jint source_width,
        jint source_height,
        jintArray pixels) {
    if (index < nevergone::choose_hero_background::kLightningFirstIndex ||
            index >= nevergone::choose_hero_background::kFrameCount) {
        return JNI_FALSE;
    }
    return nevergone::choose_hero_background::upload_from_java(
            env,
            index,
            width,
            height,
            left,
            top,
            source_width,
            source_height,
            pixels)
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ChooseHeroEffectStager_nativeEffectAssetsReady(
        JNIEnv*, jclass) {
    return nevergone::choose_hero_background::effects_ready() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeIsChooseHeroRouteActive(
        JNIEnv*, jclass) {
    // GameSurfaceView calls this once per GL frame from its existing ChooseHero
    // asset lifecycle. Preserve recovered scene ordering first, then draw the
    // role-item pane and its selected-item focus effect above the scene layers.
    nevergone::choose_hero_background_compositor::draw();
    nevergone::choose_hero_thunder_effect_compositor::draw();
    nevergone::choose_hero_black_cloud_compositor::draw();
    nevergone::choose_hero_role_item_compositor::draw();
    nevergone::choose_hero_role_focus_compositor::draw();
    return nevergone::choose_hero_background::route_active() ? JNI_TRUE : JNI_FALSE;
}
