#include "character_name_assets.h"

#include <jni.h>

#include <array>
#include <mutex>
#include <string>
#include <utility>

#include "login_localization_csv.h"

namespace nevergone::character_name_assets {
namespace {

std::mutex g_mutex;
std::array<Asset, kImageCount> g_images{};
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
    Asset result;
    result.width = width;
    result.height = height;
    result.pixels.assign(pixels, pixels + count);
    if (!valid(result)) return false;
    *output = std::move(result);
    return true;
}

bool copy_java_asset(
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
    Asset result;
    const bool ok = make_asset(
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected,
        &result);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    if (ok) *output = std::move(result);
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

bool all_ready_locked() {
    for (const Asset& asset : g_images) {
        if (!valid(asset)) return false;
    }
    for (const Asset& asset : g_labels) {
        if (!valid(asset)) return false;
    }
    return true;
}

}  // namespace

void clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (Asset& asset : g_images) asset = {};
    for (Asset& asset : g_labels) asset = {};
    ++g_generation;
}

bool upload_image(
        int slot,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (slot < 0 || slot >= kImageCount) return false;
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_images[static_cast<std::size_t>(slot)] = std::move(asset);
    ++g_generation;
    return true;
}

bool upload_label(
        int slot,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (slot < 0 || slot >= kLabelCount) return false;
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_labels[static_cast<std::size_t>(slot)] = std::move(asset);
    ++g_generation;
    return true;
}

bool ready() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return all_ready_locked();
}

std::uint64_t generation() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_generation;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Snapshot result;
    result.generation = g_generation;
    result.ready = all_ready_locked();
    result.images = g_images;
    result.labels = g_labels;
    return result;
}

}  // namespace nevergone::character_name_assets

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_CharacterNameAssetLoader_nativeClear(JNIEnv*, jclass) {
    nevergone::character_name_assets::clear();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_CharacterNameAssetLoader_nativeUploadImage(
        JNIEnv* env,
        jclass,
        jint slot,
        jint width,
        jint height,
        jintArray pixels) {
    nevergone::character_name_assets::Asset asset;
    if (!nevergone::character_name_assets::copy_java_asset(
            env, width, height, pixels, &asset)) {
        return JNI_FALSE;
    }
    return nevergone::character_name_assets::upload_image(
        static_cast<int>(slot),
        asset.width,
        asset.height,
        asset.pixels.data(),
        asset.pixels.size()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_CharacterNameAssetLoader_nativeUploadLabel(
        JNIEnv* env,
        jclass,
        jint slot,
        jint width,
        jint height,
        jintArray pixels) {
    nevergone::character_name_assets::Asset asset;
    if (!nevergone::character_name_assets::copy_java_asset(
            env, width, height, pixels, &asset)) {
        return JNI_FALSE;
    }
    return nevergone::character_name_assets::upload_label(
        static_cast<int>(slot),
        asset.width,
        asset.height,
        asset.pixels.data(),
        asset.pixels.size()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_nevergone_recomp_CharacterNameAssetLoader_nativeLabels(
        JNIEnv* env,
        jclass,
        jstring files_dir,
        jint language_column) {
    if (env == nullptr || files_dir == nullptr || language_column < 0) return nullptr;
    const std::string root = nevergone::character_name_assets::jstring_to_utf8(env, files_dir);
    const std::array<const char*, nevergone::character_name_assets::kLabelCount> keys{{
        "Prompt9", "Confirm", "Cancel"
    }};
    std::array<std::string, nevergone::character_name_assets::kLabelCount> values;
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (!nevergone::login_localization_csv::resolve_key_file(
                root,
                keys[index],
                static_cast<std::size_t>(language_column),
                &values[index]) ||
                values[index].empty()) {
            return nullptr;
        }
    }

    jclass string_class = env->FindClass("java/lang/String");
    if (string_class == nullptr) return nullptr;
    jobjectArray result = env->NewObjectArray(
        nevergone::character_name_assets::kLabelCount,
        string_class,
        nullptr);
    if (result == nullptr) {
        env->DeleteLocalRef(string_class);
        return nullptr;
    }
    for (jsize index = 0; index < nevergone::character_name_assets::kLabelCount; ++index) {
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
