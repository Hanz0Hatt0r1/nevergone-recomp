#include "choose_hero_profile_assets.h"

#include <jni.h>

#include <array>
#include <mutex>
#include <string>
#include <utility>

#include "choose_hero_profile_text.h"

namespace nevergone::choose_hero_profile_assets {
namespace {

std::mutex g_mutex;
Asset g_background;
std::array<std::array<Asset, kLabelCount>, 2> g_profiles{};
std::uint64_t g_generation = 0;

bool valid(const Asset& asset) {
    return asset.width > 0 && asset.height > 0 &&
        asset.pixels.size() ==
            static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height);
}

int slot_index(std::uint32_t slot_id) {
    if (slot_id == 1u) return 0;
    if (slot_id == 2u) return 1;
    return -1;
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
        jint width,
        jint height,
        jintArray pixels,
        Asset* output) {
    if (env == nullptr || output == nullptr || pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected) return false;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return false;
    Asset asset;
    const bool ok = make_asset(
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected,
        &asset);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    if (ok) *output = std::move(asset);
    return ok;
}

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

}  // namespace

void clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_background = {};
    for (auto& profile : g_profiles) {
        for (Asset& asset : profile) asset = {};
    }
    ++g_generation;
}

bool upload_background(
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_background = std::move(asset);
    ++g_generation;
    return true;
}

bool upload_label(
        std::uint32_t slot_id,
        int kind,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    const int slot = slot_index(slot_id);
    if (slot < 0 || kind < 0 || kind >= kLabelCount) return false;
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_profiles[static_cast<std::size_t>(slot)][static_cast<std::size_t>(kind)] = std::move(asset);
    ++g_generation;
    return true;
}

bool background_ready() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return valid(g_background);
}

bool profile_ready(std::uint32_t slot_id) {
    const int slot = slot_index(slot_id);
    if (slot < 0) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const Asset& asset : g_profiles[static_cast<std::size_t>(slot)]) {
        if (!valid(asset)) return false;
    }
    return true;
}

std::uint64_t generation() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_generation;
}

bool copy_background(Asset* output) {
    if (output == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!valid(g_background)) {
        *output = {};
        return false;
    }
    *output = g_background;
    return true;
}

bool copy_label(std::uint32_t slot_id, int kind, Asset* output) {
    if (output == nullptr || kind < 0 || kind >= kLabelCount) return false;
    const int slot = slot_index(slot_id);
    if (slot < 0) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    const Asset& asset = g_profiles[static_cast<std::size_t>(slot)][static_cast<std::size_t>(kind)];
    if (!valid(asset)) {
        *output = {};
        return false;
    }
    *output = asset;
    return true;
}

}  // namespace nevergone::choose_hero_profile_assets

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_ChooseHeroProfileLabelLoader_nativeClear(JNIEnv*, jclass) {
    nevergone::choose_hero_profile_assets::clear();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ChooseHeroProfileLabelLoader_nativeUploadBackground(
        JNIEnv* env,
        jclass,
        jint width,
        jint height,
        jintArray pixels) {
    nevergone::choose_hero_profile_assets::Asset asset;
    if (!nevergone::choose_hero_profile_assets::upload_from_java(
            env, width, height, pixels, &asset)) {
        return JNI_FALSE;
    }
    return nevergone::choose_hero_profile_assets::upload_background(
        asset.width, asset.height, asset.pixels.data(), asset.pixels.size())
        ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ChooseHeroProfileLabelLoader_nativeUploadLabel(
        JNIEnv* env,
        jclass,
        jint slot_id,
        jint kind,
        jint width,
        jint height,
        jintArray pixels) {
    nevergone::choose_hero_profile_assets::Asset asset;
    if (!nevergone::choose_hero_profile_assets::upload_from_java(
            env, width, height, pixels, &asset)) {
        return JNI_FALSE;
    }
    return nevergone::choose_hero_profile_assets::upload_label(
        static_cast<std::uint32_t>(slot_id),
        static_cast<int>(kind),
        asset.width,
        asset.height,
        asset.pixels.data(),
        asset.pixels.size())
        ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_nevergone_recomp_ChooseHeroProfileLabelLoader_nativeProfileStrings(
        JNIEnv* env,
        jclass,
        jstring files_dir,
        jint slot_id,
        jint language_column) {
    if (env == nullptr || files_dir == nullptr || language_column < 0) return nullptr;
    nevergone::choose_hero_profile_text::Text text;
    if (!nevergone::choose_hero_profile_text::load(
            nevergone::choose_hero_profile_assets::jstring_to_utf8(env, files_dir),
            static_cast<std::uint32_t>(slot_id),
            static_cast<std::size_t>(language_column),
            &text)) {
        return nullptr;
    }

    jclass string_class = env->FindClass("java/lang/String");
    if (string_class == nullptr) return nullptr;
    jobjectArray result = env->NewObjectArray(3, string_class, nullptr);
    if (result == nullptr) {
        env->DeleteLocalRef(string_class);
        return nullptr;
    }
    const std::array<std::string, 3> values{{text.name, text.level, text.play_time}};
    for (jsize index = 0; index < 3; ++index) {
        jstring value = env->NewStringUTF(values[static_cast<std::size_t>(index)].c_str());
        if (value == nullptr) {
            env->DeleteLocalRef(string_class);
            return nullptr;
        }
        env->SetObjectArrayElement(result, index, value);
        env->DeleteLocalRef(value);
    }
    env->DeleteLocalRef(string_class);
    return result;
}
