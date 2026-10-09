#include <jni.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "character_name_assets.h"
#include "login_localization_csv.h"

namespace {

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

bool upload_java_pixels(
        JNIEnv* env,
        jint slot,
        jint width,
        jint height,
        jintArray pixels,
        bool label) {
    if (env == nullptr || pixels == nullptr || width <= 0 || height <= 0) return false;
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const jsize length = env->GetArrayLength(pixels);
    if (static_cast<std::size_t>(length) != expected) return false;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return false;
    const auto* data = reinterpret_cast<const std::uint32_t*>(values);
    const bool ok = label
        ? nevergone::character_name_assets::upload_label(
              static_cast<int>(slot), static_cast<int>(width), static_cast<int>(height), data, expected)
        : nevergone::character_name_assets::upload_image(
              static_cast<int>(slot), static_cast<int>(width), static_cast<int>(height), data, expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return ok;
}

}  // namespace

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
    return upload_java_pixels(env, slot, width, height, pixels, false) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_CharacterNameAssetLoader_nativeUploadLabel(
        JNIEnv* env,
        jclass,
        jint slot,
        jint width,
        jint height,
        jintArray pixels) {
    return upload_java_pixels(env, slot, width, height, pixels, true) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_nevergone_recomp_CharacterNameAssetLoader_nativeLabels(
        JNIEnv* env,
        jclass,
        jstring files_dir,
        jint language_column) {
    if (env == nullptr || files_dir == nullptr || language_column < 0) return nullptr;
    const std::string root = jstring_to_utf8(env, files_dir);
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
