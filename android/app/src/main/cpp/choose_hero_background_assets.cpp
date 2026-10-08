#include <jni.h>

#include <array>
#include <cstdint>
#include <vector>

namespace nevergone::choose_hero_background {
namespace {

constexpr int kFrameCount = 10;
constexpr int kDesignPhaseFrameCount = 7;
constexpr int kDesignWidth = 1136;
constexpr int kDesignHeight = 640;

struct FrameAsset {
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    int source_width = 0;
    int source_height = 0;
    std::vector<std::uint32_t> pixels;
};

std::array<FrameAsset, kFrameCount> g_frames{};

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

void clear() {
    for (FrameAsset& frame : g_frames) frame = {};
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

    g_frames[static_cast<std::size_t>(index)] = std::move(frame);
    return true;
}

bool ready() {
    for (int index = 0; index < kFrameCount; ++index) {
        if (!frame_valid(g_frames[static_cast<std::size_t>(index)], index)) return false;
    }
    return true;
}

}  // namespace
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
    if (pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected) return JNI_FALSE;

    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::choose_hero_background::upload(
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
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeChooseHeroBackgroundAssetsReady(
        JNIEnv*, jclass) {
    return nevergone::choose_hero_background::ready() ? JNI_TRUE : JNI_FALSE;
}
