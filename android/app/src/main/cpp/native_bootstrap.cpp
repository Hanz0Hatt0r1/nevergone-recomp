#include "lua_bootstrap.h"

#include <jni.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <memory>
#include <mutex>
#include <sstream>
#include <string>

namespace {

std::mutex g_runtime_mutex;
std::unique_ptr<nevergone::LuaRuntime> g_lua_runtime;

const char* abi_name() {
#if defined(__aarch64__)
    return "arm64-v8a";
#elif defined(__arm__)
    return "armeabi-v7a";
#elif defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "x86";
#else
    return "unknown";
#endif
}

std::string from_jstring(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* utf = env->GetStringUTFChars(value, nullptr);
    if (utf == nullptr) {
        return {};
    }
    std::string result(utf);
    env->ReleaseStringUTFChars(value, utf);
    return result;
}

std::string bootstrap_info(
    const std::string& files_dir,
    const std::string& device_id) {
    utsname system_info{};
    const bool have_uname = uname(&system_info) == 0;
    const long page_size = sysconf(_SC_PAGESIZE);

    const std::string script_root = files_dir + "/assets/Script";
    std::string lua_status;
    {
        std::lock_guard<std::mutex> lock(g_runtime_mutex);
        if (!g_lua_runtime) {
            g_lua_runtime = std::make_unique<nevergone::LuaRuntime>(
                script_root, device_id);
        }
        lua_status = g_lua_runtime->smoke_test();
    }

    std::ostringstream out;
    out << "native bootstrap loaded\n";
    out << "ABI: " << abi_name() << "\n";
    out << "pointer width: " << (sizeof(void*) * 8) << " bit\n";
    out << "page size: " << page_size << " bytes\n";
    if (have_uname) {
        out << "kernel: " << system_info.release << "\n";
        out << "machine: " << system_info.machine << "\n";
    }
    out << "Lua: " << lua_status << "\n";
    out << "script root: " << script_root << "\n";
    out << "\nNext milestone: import decoded user-owned assets and execute Game.StartLua.";
    return out.str();
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_MainActivity_nativeBootstrapInfo(
    JNIEnv* env,
    jclass,
    jstring files_dir,
    jstring device_id) {
    const std::string info = bootstrap_info(
        from_jstring(env, files_dir),
        from_jstring(env, device_id));
    return env->NewStringUTF(info.c_str());
}
