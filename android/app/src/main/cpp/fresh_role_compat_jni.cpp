#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

#include "character_name_action_executor.h"
#include "fresh_role_compat_state.h"
#include "character_name_state.h"
#include "login_localization_csv.h"
#include "offline_hero_creation.h"
#include "offline_startup_flow.h"
#include "single_select_hero_state.h"
#include "startup_contract.h"

namespace {

void append_utf8(std::uint32_t codepoint, std::string* output) {
    if (output == nullptr) return;
    if (codepoint <= 0x7f) {
        output->push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output->push_back(static_cast<char>(0xc0u | (codepoint >> 6u)));
        output->push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    } else if (codepoint <= 0xffff) {
        output->push_back(static_cast<char>(0xe0u | (codepoint >> 12u)));
        output->push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
        output->push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    } else {
        output->push_back(static_cast<char>(0xf0u | (codepoint >> 18u)));
        output->push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3fu)));
        output->push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
        output->push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
    }
}

std::string from_java_string(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const jsize length = env->GetStringLength(value);
    const jchar* chars = env->GetStringChars(value, nullptr);
    if (chars == nullptr) return {};

    std::string result;
    result.reserve(static_cast<std::size_t>(length) * 3u);
    for (jsize index = 0; index < length; ++index) {
        std::uint32_t codepoint = chars[index];
        if (codepoint >= 0xd800u && codepoint <= 0xdbffu && index + 1 < length) {
            const std::uint32_t low = chars[index + 1];
            if (low >= 0xdc00u && low <= 0xdfffu) {
                codepoint = 0x10000u + ((codepoint - 0xd800u) << 10u) + (low - 0xdc00u);
                ++index;
            }
        }
        append_utf8(codepoint, &result);
    }
    env->ReleaseStringChars(value, chars);
    return result;
}

bool decode_utf8(const std::string& value, std::size_t* offset, std::uint32_t* codepoint) {
    if (offset == nullptr || codepoint == nullptr || *offset >= value.size()) return false;
    const auto byte = [&](std::size_t index) {
        return static_cast<unsigned char>(value[index]);
    };

    const unsigned char first = byte(*offset);
    std::size_t width = 0;
    std::uint32_t result = 0;
    if (first <= 0x7f) {
        width = 1;
        result = first;
    } else if ((first & 0xe0u) == 0xc0u) {
        width = 2;
        result = first & 0x1fu;
    } else if ((first & 0xf0u) == 0xe0u) {
        width = 3;
        result = first & 0x0fu;
    } else if ((first & 0xf8u) == 0xf0u) {
        width = 4;
        result = first & 0x07u;
    } else {
        ++(*offset);
        *codepoint = 0xfffdu;
        return true;
    }

    if (*offset + width > value.size()) {
        *offset = value.size();
        *codepoint = 0xfffdu;
        return true;
    }
    for (std::size_t index = 1; index < width; ++index) {
        const unsigned char next = byte(*offset + index);
        if ((next & 0xc0u) != 0x80u) {
            ++(*offset);
            *codepoint = 0xfffdu;
            return true;
        }
        result = (result << 6u) | (next & 0x3fu);
    }
    *offset += width;
    *codepoint = result <= 0x10ffffu ? result : 0xfffdu;
    return true;
}

jstring to_java_string(JNIEnv* env, const std::string& value) {
    if (env == nullptr) return nullptr;
    std::vector<jchar> chars;
    chars.reserve(value.size());
    std::size_t offset = 0;
    while (offset < value.size()) {
        std::uint32_t codepoint = 0;
        if (!decode_utf8(value, &offset, &codepoint)) break;
        if (codepoint <= 0xffffu && !(codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
            chars.push_back(static_cast<jchar>(codepoint));
        } else if (codepoint <= 0x10ffffu) {
            codepoint -= 0x10000u;
            chars.push_back(static_cast<jchar>(0xd800u + (codepoint >> 10u)));
            chars.push_back(static_cast<jchar>(0xdc00u + (codepoint & 0x3ffu)));
        } else {
            chars.push_back(static_cast<jchar>(0xfffdu));
        }
    }
    return env->NewString(chars.data(), static_cast<jsize>(chars.size()));
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeMode(JNIEnv*, jclass) {
    return static_cast<jint>(nevergone::fresh_role_compat_state::snapshot().mode);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeSelectedCareer(JNIEnv*, jclass) {
    return static_cast<jlong>(
        nevergone::fresh_role_compat_state::snapshot().selected_career);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeSelectCareer(
        JNIEnv*, jclass, jlong career) {
    return nevergone::fresh_role_compat_state::select_career(
               static_cast<std::int64_t>(career))
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeConfirmCareer(JNIEnv*, jclass) {
    return nevergone::fresh_role_compat_state::confirm_career() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeRoleName(JNIEnv* env, jclass) {
    return to_java_string(env, nevergone::fresh_role_compat_state::snapshot().role_name);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeRandomizePending(JNIEnv*, jclass) {
    return nevergone::fresh_role_compat_state::snapshot().randomize_pending
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeSetRoleName(
        JNIEnv* env, jclass, jstring role_name) {
    return nevergone::fresh_role_compat_state::set_role_name(
               from_java_string(env, role_name))
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeDispatchNameAction(
        JNIEnv* env, jclass, jint tag) {
    std::string error;
    const auto outcome = nevergone::character_name_action_executor::dispatch_tag(
        static_cast<int>(tag), &error);
    std::string result = nevergone::character_name_action_executor::outcome_name(outcome);
    if (!error.empty()) {
        result += ": ";
        result += error;
    }
    return to_java_string(env, result);
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeCreateOfflineHero(
        JNIEnv* env, jclass, jint language_column) {
    if (nevergone::offline_startup_flow::snapshot().route !=
            nevergone::offline_startup_flow::Route::kOpeningDialogue) {
        return to_java_string(env, "not-offline");
    }

    const auto character = nevergone::character_name_state::snapshot();
    const auto selector = nevergone::single_select_hero_state::snapshot();
    if (!character.active || !selector.active ||
            character.career != selector.selected_career) {
        return to_java_string(env, "stale-hero-selection");
    }

    const auto candidate = nevergone::offline_hero_creation::resolve(
        nevergone::startup::config().files_dir,
        character.career,
        language_column >= 0 && language_column <= 8
            ? static_cast<std::size_t>(language_column)
            : nevergone::login_localization_csv::kEnglishColumn);
    if (candidate.status != nevergone::offline_hero_creation::Status::kReady) {
        return to_java_string(
            env, nevergone::offline_hero_creation::status_name(candidate.status));
    }

    if (!nevergone::character_name_state::set_role_name(candidate.display_name)) {
        return to_java_string(env, "name-state-unavailable");
    }

    // Offline creation must not randomize the selected hero's profile name.
    if (character.randomize_pending) {
        std::int64_t randomize_career = 0;
        if (!nevergone::character_name_state::take_randomize_request(
                &randomize_career) || randomize_career != character.career) {
            return to_java_string(env, "stale-random-name-request");
        }
    }

    std::string error;
    const auto result = nevergone::character_name_action_executor::dispatch_tag(1, &error);
    std::string message =
        nevergone::character_name_action_executor::outcome_name(result);
    if (!error.empty()) message += ": " + error;
    return to_java_string(env, message);
}
