#include "lua_bootstrap.h"

#include <android/log.h>

#include <algorithm>
#include <string>
#include <utility>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace nevergone {
namespace {

constexpr const char* kLogTag = "NeverGoneRecomp";
LuaRuntime* g_runtime = nullptr;

void log_info(const std::string& message) {
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "%s", message.c_str());
}

void log_error(const std::string& message) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s", message.c_str());
}

void push_success(lua_State* state) {
    lua_pushboolean(state, 1);
}

std::string lua_error(lua_State* state, const char* prefix) {
    const char* detail = lua_tostring(state, -1);
    std::string result(prefix);
    if (detail != nullptr) {
        result += ": ";
        result += detail;
    }
    lua_pop(state, 1);
    return result;
}

bool safe_module_name(const std::string& module) {
    return !module.empty()
        && module.front() != '/'
        && module.find("..") == std::string::npos
        && module.find('\\') == std::string::npos
        && module.find(':') == std::string::npos;
}

std::string module_path(const std::string& root, std::string module) {
    if (module.size() >= 4 && module.compare(module.size() - 4, 4, ".lua") == 0) {
        module.resize(module.size() - 4);
    }
    std::replace(module.begin(), module.end(), '.', '/');

    std::string result = root;
    if (!result.empty() && result.back() != '/') {
        result.push_back('/');
    }
    result += module;
    result += ".lua";
    return result;
}

int cadd_do_string(lua_State* state) {
    const char* requested = luaL_checkstring(state, 1);
    if (g_runtime == nullptr || requested == nullptr) {
        lua_pushboolean(state, 0);
        return 1;
    }

    const std::string module(requested);
    if (!safe_module_name(module)) {
        log_error("CAddDoString rejected unsafe module name: " + module);
        lua_pushboolean(state, 0);
        return 1;
    }

    const std::string path = module_path(g_runtime->script_root(), module);
    const int load_status = luaL_loadfilex(state, path.c_str(), nullptr);
    if (load_status != LUA_OK) {
        log_error(lua_error(state, ("CAddDoString load failed for " + path).c_str()));
        lua_pushboolean(state, 0);
        return 1;
    }

    const int call_status = lua_pcall(state, 0, 0, 0);
    if (call_status != LUA_OK) {
        log_error(lua_error(state, ("CAddDoString execution failed for " + path).c_str()));
        lua_pushboolean(state, 0);
        return 1;
    }

    push_success(state);
    return 1;
}

int lua_get_platform_string(lua_State* state) {
    // The shipped Android binary contains the exact platform string "android".
    lua_pushliteral(state, "android");
    return 1;
}

int lua_get_device_uuid(lua_State* state) {
    if (g_runtime == nullptr || g_runtime->device_id().empty()) {
        lua_pushliteral(state, "recomp-device-unknown");
    } else {
        lua_pushlstring(
            state,
            g_runtime->device_id().data(),
            g_runtime->device_id().size());
    }
    return 1;
}

int cpp_show_error_dialog_ui(lua_State* state) {
    const lua_Integer code = luaL_optinteger(state, 1, 0);
    log_info("cpp_ShowErrorDialogUI stub: code=" + std::to_string(code));
    push_success(state);
    return 1;
}

int cpp_show_loading_ui(lua_State* state) {
    (void)state;
    log_info("cpp_ShowLoadingUI stub");
    push_success(state);
    return 1;
}

int cpp_hide_loading_ui(lua_State* state) {
    (void)state;
    log_info("cpp_HideLoadingUI stub");
    push_success(state);
    return 1;
}

int cpp_show_message_box_ui(lua_State* state) {
    const char* first = luaL_optstring(state, 1, "");
    const char* second = luaL_optstring(state, 2, "");
    const char* third = luaL_optstring(state, 3, "");
    const char* fourth = luaL_optstring(state, 4, "");
    log_info(
        std::string("cpp_ShowMessageBoxUI stub: ")
        + first + " | " + second + " | " + third + " | " + fourth);
    push_success(state);
    return 1;
}

void register_function(lua_State* state, const char* name, lua_CFunction function) {
    lua_pushcfunction(state, function);
    lua_setglobal(state, name);
}

void register_startup_contract(lua_State* state) {
    register_function(state, "CAddDoString", cadd_do_string);
    register_function(state, "Lua_GetPlatformString", lua_get_platform_string);
    register_function(state, "Lua_GetDeviceUUID", lua_get_device_uuid);
    register_function(state, "cpp_ShowErrorDialogUI", cpp_show_error_dialog_ui);
    register_function(state, "cpp_ShowLoadingUI", cpp_show_loading_ui);
    register_function(state, "cpp_HideLoadingUI", cpp_hide_loading_ui);
    register_function(state, "cpp_ShowMessageBoxUI", cpp_show_message_box_ui);
}

}  // namespace

LuaRuntime::LuaRuntime(std::string script_root, std::string device_id)
    : script_root_(std::move(script_root)), device_id_(std::move(device_id)) {}

LuaRuntime::~LuaRuntime() {
    if (g_runtime == this) {
        g_runtime = nullptr;
    }
    if (state_ != nullptr) {
        lua_close(state_);
        state_ = nullptr;
    }
}

bool LuaRuntime::initialize() {
    if (state_ != nullptr) {
        return true;
    }

    state_ = luaL_newstate();
    if (state_ == nullptr) {
        last_error_ = "luaL_newstate failed";
        return false;
    }

    g_runtime = this;
    luaL_openlibs(state_);
    register_startup_contract(state_);
    return true;
}

bool LuaRuntime::ready() const {
    return state_ != nullptr;
}

std::string LuaRuntime::smoke_test() {
    if (!initialize()) {
        return "Lua bootstrap failed: " + last_error_;
    }

    static constexpr const char* kSmokeScript = R"lua(
assert(_VERSION == "Lua 5.2")
assert(Lua_GetPlatformString() == "android")
assert(type(Lua_GetDeviceUUID()) == "string")
assert(type(CAddDoString) == "function")
assert(cpp_ShowLoadingUI() == true)
assert(cpp_HideLoadingUI() == true)
return "Lua 5.2.3 startup contract: OK"
)lua";

    const int load_status = luaL_loadbufferx(
        state_, kSmokeScript, std::char_traits<char>::length(kSmokeScript),
        "@recomp_bootstrap_smoke", nullptr);
    if (load_status != LUA_OK) {
        last_error_ = lua_error(state_, "smoke load failed");
        return "Lua bootstrap failed: " + last_error_;
    }

    const int call_status = lua_pcall(state_, 0, 1, 0);
    if (call_status != LUA_OK) {
        last_error_ = lua_error(state_, "smoke execution failed");
        return "Lua bootstrap failed: " + last_error_;
    }

    const char* result = lua_tostring(state_, -1);
    std::string message = result != nullptr ? result : "Lua smoke returned no text";
    lua_pop(state_, 1);
    last_error_.clear();
    return message;
}

const std::string& LuaRuntime::last_error() const {
    return last_error_;
}

const std::string& LuaRuntime::script_root() const {
    return script_root_;
}

const std::string& LuaRuntime::device_id() const {
    return device_id_;
}

}  // namespace nevergone
