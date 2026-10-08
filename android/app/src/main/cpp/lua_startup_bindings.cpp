#include "lua_startup_bindings.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>

#include "startup_contract.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif

namespace nevergone::lua_runtime {

#if defined(NEVERGONE_HAS_LUA)
namespace {

bool module_to_path(const std::string& module_name, std::string* output, std::string* error) {
    if (module_name.empty()) {
        if (error != nullptr) {
            *error = "empty module name";
        }
        return false;
    }
    if (module_name.front() == '/' || module_name.front() == '\\' ||
        module_name.find("..") != std::string::npos) {
        if (error != nullptr) {
            *error = "unsafe module path";
        }
        return false;
    }

    std::string relative = module_name;
    std::replace(relative.begin(), relative.end(), '\\', '/');

    const bool has_lua_suffix =
        relative.size() >= 4 && relative.substr(relative.size() - 4) == ".lua";
    if (!has_lua_suffix) {
        std::replace(relative.begin(), relative.end(), '.', '/');
        relative += ".lua";
    }

    const std::filesystem::path root =
        std::filesystem::path(startup::config().files_dir) / "assets" / "Script";
    const std::filesystem::path candidate = (root / relative).lexically_normal();
    const std::filesystem::path normalized_root = root.lexically_normal();

    const std::string root_string = normalized_root.string();
    const std::string candidate_string = candidate.string();
    if (candidate_string.compare(0, root_string.size(), root_string) != 0) {
        if (error != nullptr) {
            *error = "module path escapes script root";
        }
        return false;
    }

    *output = candidate_string;
    return true;
}

int l_Lua_GetPlatformString(lua_State* state) {
    const std::string value = startup::Lua_GetPlatformString();
    lua_pushlstring(state, value.data(), value.size());
    return 1;
}

int l_Lua_GetDeviceUUID(lua_State* state) {
    const std::string value = startup::Lua_GetDeviceUUID();
    lua_pushlstring(state, value.data(), value.size());
    return 1;
}

int l_cpp_ShowLoadingUI(lua_State*) {
    startup::cpp_ShowLoadingUI();
    return 0;
}

int l_cpp_HideLoadingUI(lua_State*) {
    startup::cpp_HideLoadingUI();
    return 0;
}

int l_cpp_ShowErrorDialogUI(lua_State* state) {
    const char* message = luaL_optstring(state, 1, "");
    startup::cpp_ShowErrorDialogUI(message != nullptr ? message : "");
    return 0;
}

int l_cpp_ShowMessageBoxUI(lua_State* state) {
    const char* title = luaL_optstring(state, 1, "");
    const char* message = luaL_optstring(state, 2, "");
    startup::cpp_ShowMessageBoxUI(
        title != nullptr ? title : "",
        message != nullptr ? message : "");
    return 0;
}

int l_CAddDoString(lua_State* state) {
    const char* module_name = luaL_checkstring(state, 1);
    if (module_name == nullptr || *module_name == '\0') {
        return luaL_error(state, "CAddDoString: empty module name");
    }

    startup::CAddDoString(module_name);
    std::string error;
    if (!execute_module(state, module_name, &error)) {
        return luaL_error(state, "CAddDoString(%s): %s", module_name, error.c_str());
    }
    return 0;
}

void set_global(lua_State* state, const char* name, lua_CFunction function) {
    lua_pushcfunction(state, function);
    lua_setglobal(state, name);
}

}  // namespace

void register_startup_bindings(lua_State* state) {
    set_global(state, "CAddDoString", l_CAddDoString);
    set_global(state, "Lua_GetPlatformString", l_Lua_GetPlatformString);
    set_global(state, "Lua_GetDeviceUUID", l_Lua_GetDeviceUUID);
    set_global(state, "cpp_ShowLoadingUI", l_cpp_ShowLoadingUI);
    set_global(state, "cpp_HideLoadingUI", l_cpp_HideLoadingUI);
    set_global(state, "cpp_ShowErrorDialogUI", l_cpp_ShowErrorDialogUI);
    set_global(state, "cpp_ShowMessageBoxUI", l_cpp_ShowMessageBoxUI);
}

bool execute_module(lua_State* state, const std::string& module_name, std::string* error) {
    std::string path;
    if (!module_to_path(module_name, &path, error)) {
        return false;
    }

    int status = luaL_loadfilex(state, path.c_str(), nullptr);
    if (status == 0) {
        status = lua_pcall(state, 0, 0, 0);
    }
    if (status == 0) {
        return true;
    }

    const char* message = lua_tostring(state, -1);
    if (error != nullptr) {
        *error = message != nullptr ? message : "unknown Lua error";
    }
    lua_pop(state, 1);
    return false;
}
#endif

std::string startup_execution_report() {
#if defined(NEVERGONE_HAS_LUA)
    const std::filesystem::path start_lua =
        std::filesystem::path(startup::config().files_dir) /
        "assets" / "Script" / "Game" / "StartLua.lua";

    if (!std::filesystem::is_regular_file(start_lua)) {
        return "startup script: not imported\n";
    }

    lua_State* state = luaL_newstate();
    if (state == nullptr) {
        return "startup script: failed (luaL_newstate)\n";
    }
    luaL_openlibs(state);
    register_startup_bindings(state);

    std::string error;
    const bool ok = execute_module(state, "Game.StartLua", &error);
    lua_close(state);

    std::ostringstream out;
    out << "startup script: " << (ok ? "executed" : "failed") << "\n";
    if (!ok) {
        out << "startup error: " << error << "\n";
    }
    return out.str();
#else
    return "startup script: skipped (Lua unavailable)\n";
#endif
}

}  // namespace nevergone::lua_runtime
