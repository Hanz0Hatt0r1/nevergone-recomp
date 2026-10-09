#include "choose_hero_action_control_assets.h"

#include <jni.h>

#include <array>
#include <mutex>
#include <string>
#include <utility>

#include "login_localization_csv.h"

namespace nevergone::choose_hero_action_control_assets {
namespace {

std::mutex g_mutex;
std::array<Asset, kButtonAssetCount> g_buttons{};
std::array<Asset, kLabelCount> g_labels{};
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

bool upload_java_asset(
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
    for (Asset& asset : g_buttons) asset = {};
    for (Asset& asset : g_labels) asset = {};
    ++g_generation;
}

bool upload_button(
        int index,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (index < 0 || index >= kButtonAssetCount) return false;
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_buttons[static_cast<std::size_t>(index)] = std::move(asset);
    ++g_generation;
    return true;
}

bool upload_label(
        int index,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (index < 0 || index >= kLabelCount) return false;
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_labels[static_cast<std::size_t>(index)] = std::move(asset);
    ++g_generation;
    return true;
}

bool ready() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const Asset& asset : g_buttons) if (!valid(asset)) return false;
    for (const Asset& asset : g_labels) if (!valid(asset)) return false;
    return true;
}

std::uint64_t generation() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_generation;
}

bool copy_button(int index, Asset* output) {
    if (output == nullptr || index < 0 || index >= kButtonAssetCount) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    const Asset& asset = g_buttons[static_cast<std::size_t>(index)];
    if (!valid(asset)) {
        *output = {};
        return false;
    }
    *output = asset;
    return true;
}

bool copy_label(int index, Asset* output) {
    if (output == nullptr || index < 0 || index >= kLabelCount) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    const Asset& asset = g_labels[static_cast<std::size_t>(index)];
    if (!valid(asset)) {
        *output = {};
        return false;
    }
    *output = asset;
    return true;
}

}  // namespace nevergone::choose_hero_action_control_assets

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_ChooseHeroActionControlLoader_nativeClear(JNIEnv*, jclass) {
    nevergone::choose_hero_action_control_assets::clear();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ChooseHeroActionControlLoader_nativeUploadButton(
        JNIEnv* env,
        jclass,
        jint index,
        jint width,
        jint height,
        jintArray pixels) {
    nevergone::choose_hero_action_control_assets::Asset asset;
    if (!nevergone::choose_hero_action_control_assets::upload_java_asset(
            env, width, height, pixels, &asset)) {
        return JNI_FALSE;
    }
    return nevergone::choose_hero_action_control_assets::upload_button(
        static_cast<int>(index),
        asset.width,
        asset.height,
        asset.pixels.data(),
        asset.pixels.size()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_ChooseHeroActionControlLoader_nativeUploadLabel(
        JNIEnv* env,
        jclass,
        jint index,
        jint width,
        jint height,
        jintArray pixels) {
    nevergone::choose_hero_action_control_assets::Asset asset;
    if (!nevergone::choose_hero_action_control_assets::upload_java_asset(
            env, width, height, pixels, &asset)) {
        return JNI_FALSE;
    }
    return nevergone::choose_hero_action_control_assets::upload_label(
        static_cast<int>(index),
        asset.width,
        asset.height,
        asset.pixels.data(),
        asset.pixels.size()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_nevergone_recomp_ChooseHeroActionControlLoader_nativeControlStrings(
        JNIEnv* env,
        jclass,
        jstring files_dir,
        jint language_column) {
    if (env == nullptr || files_dir == nullptr || language_column < 0) return nullptr;
    const std::string root = nevergone::choose_hero_action_control_assets::jstring_to_utf8(
        env, files_dir);
    std::string play;
    std::string delete_hero;
    if (!nevergone::login_localization_csv::resolve_key_file(
            root, "PlayGame", static_cast<std::size_t>(language_column), &play) ||
            !nevergone::login_localization_csv::resolve_key_file(
                root, "Prompt21", static_cast<std::size_t>(language_column), &delete_hero)) {
        return nullptr;
    }

    jclass string_class = env->FindClass("java/lang/String");
    if (string_class == nullptr) return nullptr;
    jobjectArray result = env->NewObjectArray(2, string_class, nullptr);
    if (result == nullptr) {
        env->DeleteLocalRef(string_class);
        return nullptr;
    }
    const std::array<std::string, 2> values{{play, delete_hero}};
    for (jsize index = 0; index < 2; ++index) {
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
