#include "lua_runtime.h"

#include <filesystem>
#include <sstream>

#include "native_binding_registry.h"
#include "startup_contract.h"

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

    const std::filesystem::path xml_smoke_path =
        std::filesystem::path(startup::config().files_dir) / "xml-smoke.xml";
    const std::string xml_smoke_path_string = xml_smoke_path.string();
    lua_pushlstring(state, xml_smoke_path_string.data(), xml_smoke_path_string.size());
    lua_setglobal(state, "NeverGoneXmlSmokePath");

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
        "local encoded = cjson.encode({name='Never Gone', count=3, flags={true, false}, quote=[[a\"b]]}); "
        "local decoded = cjson.decode(encoded); "
        "local with_null = cjson.decode([[{\"value\":null}]]); "
        "local cjson_ok = decoded.name == 'Never Gone' and decoded.count == 3 and "
        "decoded.flags[1] == true and decoded.flags[2] == false and decoded.quote == [[a\"b]] and "
        "with_null.value == cjson.null; "
        "local nickname_ok = Lua_CheckNickName('Hero7') and Lua_CheckNickName('勇者7') and "
        "not Lua_CheckNickName('bad_name') and not Lua_CheckNickName('hero!') and "
        "not Lua_CheckNickName('🙂') and not Lua_CheckNickName(''); "
        "local module_ok = false; "
        "do local registered_xml = xml; registered_xml.native = true; "
        "local function chunk() module('xml'); value = 7 end; chunk(); "
        "module_ok = package.loaded.xml == registered_xml and xml == registered_xml and "
        "registered_xml.value == 7 and registered_xml.native == true end; "
        "local escaped = xml.encode([[<tag a=\"b\">&]]); "
        "local saved = xml._save([[<?xml version=\"1.0\"?><root a=\"1\"><child>text &amp; more</child><empty /></root>]], NeverGoneXmlSmokePath); "
        "local loaded = xml.load(NeverGoneXmlSmokePath); "
        "local xml_ok = saved == true and escaped == [[&lt;tag a=&quot;b&quot;&gt;&amp;]] and "
        "loaded ~= nil and loaded[0] == 'root' and loaded.a == '1' and "
        "loaded[1] ~= nil and loaded[1][0] == 'child' and loaded[1][1] == 'text & more' and "
        "loaded[2] ~= nil and loaded[2][0] == 'empty'; "
        "return uuid_ok, imported == true and connected == false, unpack_ok, bit_ok, cjson_ok, nickname_ok, module_ok, xml_ok");
    if (status == 0) {
        status = lua_pcall(state, 0, 8, 0);
    }
    const bool uuid_alias_ok = status == 0 && lua_toboolean(state, -8) != 0;
    const bool protorpc_ok = status == 0 && lua_toboolean(state, -7) != 0;
    const bool unpack_ok = status == 0 && lua_toboolean(state, -6) != 0;
    const bool bit_ok = status == 0 && lua_toboolean(state, -5) != 0;
    const bool cjson_ok = status == 0 && lua_toboolean(state, -4) != 0;
    const bool nickname_ok = status == 0 && lua_toboolean(state, -3) != 0;
    const bool module_ok = status == 0 && lua_toboolean(state, -2) != 0;
    const bool xml_ok = status == 0 && lua_toboolean(state, -1) != 0;

    std::error_code cleanup_error;
    std::filesystem::remove(xml_smoke_path, cleanup_error);

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
        << ((basic_ok && uuid_alias_ok && protorpc_ok && unpack_ok && bit_ok && cjson_ok &&
             nickname_ok && module_ok && xml_ok && probe_ok)
                ? "ok"
                : "failed")
        << "\n";
    out << "lua runtime: " << runtime_version_copy << "\n";
    out << "device UUID alias: " << (uuid_alias_ok ? "ok" : "failed") << "\n";
    out << "ProtoRPC shell: " << (protorpc_ok ? "ok" : "failed") << "\n";
    out << "global unpack compat: " << (unpack_ok ? "ok" : "failed") << "\n";
    out << "legacy bit compat: " << (bit_ok ? "ok" : "failed") << "\n";
    out << "cjson compat: " << (cjson_ok ? "ok" : "failed") << "\n";
    out << "nickname validation: " << (nickname_ok ? "ok" : "failed") << "\n";
    out << "module() compat: " << (module_ok ? "ok" : "failed") << "\n";
    out << "LuaXML compat: " << (xml_ok ? "ok" : "failed") << "\n";
    out << "missing-global probe: " << (probe_ok ? "ok" : "failed") << "\n";
    lua_close(state);
    return out.str();
#else
    return "lua smoke test: skipped (run tools/fetch_lua_5_2_3.py before build)\n";
#endif
}

}  // namespace nevergone::lua_runtime
