#include "lua_compat.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {

void install_lua51_compat(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    // Never Gone ships Lua 5.2.3 but recovered scripts still use a few legacy
    // Lua 5.1-era globals plus the old `bit` module name. Install only the
    // compatibility shims actually referenced by the client scripts.
    constexpr const char* kCompatScript = R"LUA(
if unpack == nil and table ~= nil then
    unpack = table.unpack
end

if bit == nil and bit32 ~= nil then
    bit = bit32
end

if module == nil then
    function module(name)
        assert(type(name) == "string" and name ~= "", "module name expected")

        local target = package.loaded[name]
        if type(target) ~= "table" then
            target = rawget(_G, name)
        end
        if type(target) ~= "table" then
            target = {}
        end

        package.loaded[name] = target
        rawset(_G, name, target)
        target._M = target
        target._NAME = name
        target._PACKAGE = string.match(name, "^(.*%.)") or ""

        local caller = debug.getinfo(2, "f")
        if caller ~= nil and caller.func ~= nil then
            local index = 1
            while true do
                local upvalue = debug.getupvalue(caller.func, index)
                if upvalue == nil then
                    break
                end
                if upvalue == "_ENV" then
                    debug.setupvalue(caller.func, index, target)
                    break
                end
                index = index + 1
            end
        end

        return target
    end
end
)LUA";

    if (luaL_loadstring(state, kCompatScript) != 0) {
        lua_pop(state, 1);
        return;
    }
    if (lua_pcall(state, 0, 0, 0) != 0) {
        lua_pop(state, 1);
    }
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
