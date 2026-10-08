#include "lua_runtime.h"

#include <sstream>

#include "native_binding_registry.h"

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
    if (status == 0) {
        status = lua_pcall(state, 0, 2, 0);
    }

    std::ostringstream out;
    if (status != 0) {
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
    const bool basic_ok = runtime_version != nullptr && answer == 42;
    const std::string runtime_version_copy =
        runtime_version != nullptr ? runtime_version : "unknown";

    lua_settop(state, 0);
    register_native_bindings(state);
    status = luaL_loadstring(
        state,
        "local p = ProtoRPC:new(); "
        "p:SetID('smoke'); "
        "p:SetProtoFileRootDir('conf'); "
        "local imported = p:ImportProtoFile('kClientCommon.proto'); "
        "local connected = p:CheckConnection(false); "
        "p:Close(); p:release(); "
        "return imported == true and connected == false");
    if (status == 0) {
        status = lua_pcall(state, 0, 1, 0);
    }
    const bool protorpc_ok = status == 0 && lua_toboolean(state, -1) != 0;

    lua_settop(state, 0);
    install_missing_global_probe(state);
    status = luaL_loadstring(state, "return NeverGoneMissingProbe == nil");
    if (status == 0) {
        status = lua_pcall(state, 0, 1, 0);
    }

    bool probe_ok = false;
    if (status == 0) {
        const bool returned_nil_semantics = lua_toboolean(state, -1) != 0;
        const auto missing = take_missing_globals();
        probe_ok = returned_nil_semantics && missing.size() == 1 &&
                   missing.front().name == "NeverGoneMissingProbe" &&
                   missing.front().hits == 1;
    } else {
        take_missing_globals();
    }

    out << "lua smoke test: " << ((basic_ok && protorpc_ok && probe_ok) ? "ok" : "failed") << "\n";
    out << "lua runtime: " << runtime_version_copy << "\n";
    out << "ProtoRPC shell: " << (protorpc_ok ? "ok" : "failed") << "\n";
    out << "missing-global probe: " << (probe_ok ? "ok" : "failed") << "\n";
    lua_close(state);
    return out.str();
#else
    return "lua smoke test: skipped (run tools/fetch_lua_5_2_3.py before build)\n";
#endif
}

}  // namespace nevergone::lua_runtime
