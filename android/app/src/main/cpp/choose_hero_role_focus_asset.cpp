#include "choose_hero_role_focus_asset.h"

#include <jni.h>

#include <mutex>
#include <utility>

namespace nevergone::choose_hero_role_focus_asset {
namespace {

std::mutex g_mutex;
Asset g_asset;
std::uint64_t g_generation = 0;

bool valid(const Asset& asset) {
    return asset.width > 0 && asset.height > 0 &&
        asset.pixels.size() ==
            static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height);
}

}  // namespace

void clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_asset = {};
    ++g_generation;
}

bool upload(
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (width <= 0 || height <= 0 || pixels == nullptr ||
            count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
            count > 16777216u) {
        return false;
    }
    Asset asset;
    asset.width = width;
    asset.height = height;
    asset.pixels.assign(pixels, pixels + count);
    if (!valid(asset)) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_asset = std::move(asset);
    ++g_generation;
    return true;
}

bool ready() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return valid(g_asset);
}

std::uint64_t generation() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_generation;
}

bool copy(Asset* output) {
    if (output == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!valid(g_asset)) {
        *output = {};
        return false;
    }
    *output = g_asset;
    return true;
}

}  // namespace nevergone::choose_hero_role_focus_asset

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_ChooseHeroRoleFocusAssetLoader_nativeClear(JNIEnv*, jclass) {
    nevergone::choose_hero_role_focus_asset::clear();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ChooseHeroRoleFocusAssetLoader_nativeUpload(
        JNIEnv* env,
        jclass,
        jint width,
        jint height,
        jintArray pixels) {
    if (pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected) return JNI_FALSE;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::choose_hero_role_focus_asset::upload(
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ChooseHeroRoleFocusAssetLoader_nativeReady(JNIEnv*, jclass) {
    return nevergone::choose_hero_role_focus_asset::ready() ? JNI_TRUE : JNI_FALSE;
}
