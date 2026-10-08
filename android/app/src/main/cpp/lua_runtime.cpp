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
        "local uuid_ok = Lua_GetDeviceUUID() == LGG_Device_UUID(); "
        "local p = ProtoRPC:new(); "
        "p:SetID('smoke'); "
        "p:SetProtoFileRootDir('conf'); "
        "local imported = p:ImportProtoFile('kClientCommon.proto'); "
        "local connected = p:CheckConnection(false); "
        "p:Close(); p:release(); "
        "local unpack_ok = unpack ~= nil and unpack({4, 5}) == 4; "
        "local bit_ok = bit ~= nil and bit.band(0xf3, 0x0f) == 3 and "
        "bit.bor(1, 4) == 5 and bit.lshift(1, 4) == 16 and bit.rshift(16, 4) == 1; "
        "local module_ok = false; "
        "do local xml = { native = true }; _G.xml = xml; "
        "local function chunk() module('xml'); value = 7 end; chunk(); "
        "module_ok = package.loaded.xml == xml and xml.value == 7 and xml.native == true end; "
        "return uuid_ok, imported == true and connected == false, unpack_ok, bit_ok, module_ok");
    if (status == 0) {
        status = lua_pcall(state, 0, 5, 0);
    }
    const bool uuid_alias_ok = status == 0 && lua_toboolean(state, -5) != 0;
    const bool protorpc_ok = status == 0 && lua_toboolean(state, -4) != 0;
    const bool unpack_ok = status == 0 && lua_toboolean(state, -3) != 0;
    const bool bit_ok = status == 0 && lua_toboolean(state, -2) != 0;
    const bool module_ok = status == 0 && lua_toboolean(state, -1) != 0;

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

    out << "lua smoke test: "
        << ((basic_ok && uuid_alias_ok && protorpc_ok && unpack_ok && bit_ok && module_ok && probe_ok)
                ? "ok"
                : "failed")
        << "\n";
    out << "lua runtime: " << runtime_version_copy << "\n";
    out << "device UUID alias: " << (uuid_alias_ok ? "ok" : "failed") << "\n";
    out << "ProtoRPC shell: " << (protorpc_ok ? "ok" : "failed") << "\n";
    out << "global unpack compat: " << (unpack_ok ? "ok" : "failed") << "\n";
    out << "legacy bit compat: " << (bit_ok ? "ok" : "failed") << "\n";
    out << "module() compat: " << (module_ok ? "ok" : "failed") << "\n";
    out << "missing-global probe: " << (probe_ok ? "ok" : "failed") << "\n";
    lua_close(state);
    return out.str();
#else
    return "lua smoke test: skipped (run tools/fetch_lua_5_2_3.py before build)\n";
#endif
}

}  // namespace nevergone::lua_runtime
