#include "lua_runtime.h"

#include <sstream>

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif

namespace nevergone::lua_runtime {

bool available() {
#if defined(NEVERGONE_HAS_LUA)
    return true;
#else
    return false;
#endif
}

std::string version() {
#if defined(NEVERGONE_HAS_LUA)
    return LUA_RELEASE;
#else
    return "Lua unavailable";
#endif
}

std::string smoke_test() {
#if defined(NEVERGONE_HAS_LUA)
    lua_State* state = luaL_newstate();
    if (state == nullptr) {
        return "lua smoke test: failed (luaL_newstate)\n";
    }

    luaL_openlibs(state);
    constexpr const char* script = "return _VERSION, 6 * 7";
    int status = luaL_loadstring(state, script);
    if (status == LUA_OK) {
        status = lua_pcall(state, 0, 2, 0);
    }

    std::ostringstream out;
    if (status != LUA_OK) {
        const char* error = lua_tostring(state, -1);
        out << "lua smoke test: failed";
        if (error != nullptr) {
            out << " (" << error << ")";
        }
        out << "\n";
        lua_close(state);
        return out.str();
    }

    const char* runtime_version = lua_tostring(state, -2);
    const lua_Number answer = lua_tonumber(state, -1);
    const bool ok = runtime_version != nullptr && answer == 42;

    out << "lua smoke test: " << (ok ? "ok" : "failed") << "\n";
    out << "lua runtime: " << (runtime_version != nullptr ? runtime_version : "unknown") << "\n";
    lua_close(state);
    return out.str();
#else
    return "lua smoke test: skipped (run tools/fetch_lua_5_2_3.py before build)\n";
#endif
}

}  // namespace nevergone::lua_runtime
