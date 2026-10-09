#include <jni.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <utility>

#include "app_delegate_state.h"
#include "choose_hero_background_compositor.h"
#include "choose_hero_black_cloud_compositor.h"
#include "choose_hero_thunder_effect_compositor.h"
#include "client_callback_bridge.h"
#include "game_levels_asset_probe.h"
#include "initial_ui_transition.h"
#include "login_lua_session.h"
#include "lua_runtime.h"
#include "offline_startup_flow.h"
#include "render_bridge.h"
#include "server_selection_compositor.h"
#include "server_selection_state.h"
#include "startup_contract.h"

namespace {

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

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

std::string bootstrap_info() {
    utsname system_info{};
    const bool have_uname = uname(&system_info) == 0;
    const long page_size = sysconf(_SC_PAGESIZE);

    // Start or retry the long-lived reconstructed Lua state. A failed start is
    // retained in diagnostics; importing assets and refreshing can retry it.
    (void)nevergone::login_lua_session::ensure_started();

    std::ostringstream out;
    out << "native bootstrap loaded\n";
    out << "ABI: " << abi_name() << "\n";
    out << "pointer width: " << (sizeof(void*) * 8) << " bit\n";
    out << "page size: " << page_size << " bytes\n";
    if (have_uname) {
        out << "kernel: " << system_info.release << "\n";
        out << "machine: " << system_info.machine << "\n";
    }

    const auto& runtime = nevergone::startup::config();
    out << "files dir configured: " << (!runtime.files_dir.empty() ? "yes" : "no") << "\n";
    out << "app version: " << (runtime.app_version.empty() ? "unknown" : runtime.app_version) << "\n";
    out << "lua runtime: " << nevergone::lua_runtime::version() << "\n";
    out << nevergone::app_delegate_state::status_report();
    out << nevergone::initial_ui_transition::status_report();
    out << nevergone::render::status_report();
    out << nevergone::server_selection_state::status_report();
    out << nevergone::server_selection_compositor::status_report();
    out << nevergone::choose_hero_background_compositor::status_report();
    out << nevergone::choose_hero_thunder_effect_compositor::status_report();
    out << nevergone::choose_hero_black_cloud_compositor::status_report();
    out << nevergone::login_lua_session::status_report();
    out << nevergone::offline_startup_flow::status_report();
    out << nevergone::game_levels_asset_probe::status_report(runtime.files_dir);
    out << nevergone::startup::smoke_test_report();
    out << nevergone::login_lua_session::startup_report();
    out << "\nNext milestone: route recovered ChooseHero thunder sound callbacks to imported audio, then continue role-selection/scene-entry reconstruction.";
    return out.str();
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_MainActivity_nativeConfigureRuntime(
    JNIEnv* env,
    jclass,
    jstring files_dir,
    jstring device_id,
    jstring app_version) {
    nevergone::login_lua_session::shutdown();
    nevergone::startup::RuntimeConfig config;
    config.files_dir = jstring_to_utf8(env, files_dir);
    config.device_id = jstring_to_utf8(env, device_id);
    config.app_version = jstring_to_utf8(env, app_version);
    config.platform = "android";
    nevergone::startup::configure(std::move(config));
    nevergone::app_delegate_state::on_runtime_configured();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_MainActivity_nativeAppDelegateOnPause(JNIEnv*, jclass) {
    nevergone::app_delegate_state::on_app_pause();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_MainActivity_nativeAppDelegateOnResume(JNIEnv*, jclass) {
    nevergone::app_delegate_state::on_app_resume();
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_MainActivity_nativeBootstrapInfo(JNIEnv* env, jclass) {
    const std::string info = bootstrap_info();
    return env->NewStringUTF(info.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_MainActivity_nativeClientUiState(JNIEnv* env, jclass) {
    const std::string report = nevergone::lua_runtime::client_ui_state_report();
    return env->NewStringUTF(report.c_str());
}
