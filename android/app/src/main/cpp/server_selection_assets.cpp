#include "server_selection_assets.h"

#include <jni.h>

#include <array>
#include <mutex>
#include <utility>

namespace nevergone::server_selection_assets {
namespace {

std::mutex g_mutex;
std::array<Asset, kAssetCount> g_assets{};
std::uint64_t g_generation = 0;

bool valid(const Asset& asset) {
    return asset.width > 0 && asset.height > 0 &&
        asset.pixels.size() ==
            static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height);
}

bool make_asset(
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count,
        Asset* output) {
    if (output == nullptr || width <= 0 || height <= 0 || pixels == nullptr ||
            count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
            count > 16777216u) {
        return false;
    }
    Asset asset;
    asset.width = width;
    asset.height = height;
    asset.pixels.assign(pixels, pixels + count);
    if (!valid(asset)) return false;
    *output = std::move(asset);
    return true;
}

bool upload_from_java(
        JNIEnv* env,
        jint index,
        jint width,
        jint height,
        jintArray pixels) {
    if (env == nullptr || pixels == nullptr || index < 0 || index >= kAssetCount ||
            width <= 0 || height <= 0) {
        return false;
    }
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected) return false;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return false;
    const bool ok = upload(
        static_cast<int>(index),
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return ok;
}

}  // namespace

void clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (Asset& asset : g_assets) asset = {};
    ++g_generation;
}

bool upload(
        int index,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (index < 0 || index >= kAssetCount) return false;
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_assets[static_cast<std::size_t>(index)] = std::move(asset);
    ++g_generation;
    return true;
}

bool ready(int index) {
    if (index < 0 || index >= kAssetCount) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    return valid(g_assets[static_cast<std::size_t>(index)]);
}

bool row_ready() {
    return ready(kBorder2);
}

bool confirm_ready() {
    return ready(kButtonNormal) && ready(kButtonPressed) && ready(kStartLabel);
}

bool selector_labels_ready() {
    return ready(kZoneSuffixLabel) && ready(kChooseZoneLabel);
}

std::uint64_t generation() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_generation;
}

bool dimensions(int index, int* width, int* height) {
    if (width == nullptr || height == nullptr || index < 0 || index >= kAssetCount) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    const Asset& asset = g_assets[static_cast<std::size_t>(index)];
    if (!valid(asset)) {
        *width = 0;
        *height = 0;
        return false;
    }
    *width = asset.width;
    *height = asset.height;
    return true;
}

bool copy(int index, Asset* output) {
    if (output == nullptr || index < 0 || index >= kAssetCount) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    const Asset& asset = g_assets[static_cast<std::size_t>(index)];
    if (!valid(asset)) {
        *output = {};
        return false;
    }
    *output = asset;
    return true;
}

}  // namespace nevergone::server_selection_assets

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_ServerSelectionAssetLoader_nativeClear(JNIEnv*, jclass) {
    nevergone::server_selection_assets::clear();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ServerSelectionAssetLoader_nativeUpload(
        JNIEnv* env,
        jclass,
        jint index,
        jint width,
        jint height,
        jintArray pixels) {
    return nevergone::server_selection_assets::upload_from_java(
        env, index, width, height, pixels) ? JNI_TRUE : JNI_FALSE;
}
