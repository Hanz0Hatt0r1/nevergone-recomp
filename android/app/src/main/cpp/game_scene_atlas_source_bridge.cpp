#include <jni.h>

#include <string>

#include "filesystem_bindings.h"

namespace {

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneAtlasSourceResolver_nativeResolveResourcePath(
        JNIEnv* env,
        jclass,
        jstring resource_name) {
    const std::string name = jstring_to_utf8(env, resource_name);
    if (name.empty()) return nullptr;
    const std::string resolved = nevergone::lua_runtime::resolve_resource_path(name);
    if (resolved.empty()) return nullptr;
    return env->NewStringUTF(resolved.c_str());
}
