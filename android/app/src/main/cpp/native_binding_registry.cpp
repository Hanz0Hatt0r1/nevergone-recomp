#include "native_binding_registry.h"

#include <map>
#include <mutex>

#include "filesystem_bindings.h"
#include "lua_compat.h"
#include "lua_startup_bindings.h"
#include "protorpc_bindings.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {
namespace {

std::mutex g_missing_mutex;
std::map<std::string, int> g_missing_globals;

#if defined(NEVERGONE_HAS_LUA)
int missing_global_index(lua_State* state) {
    if (lua_type(state, 2) == LUA_TSTRING) {
        size_t length = 0;
        const char* name = lua_tolstring(state, 2, &length);
        if (name != nullptr && length != 0) {
            std::lock_guard<std::mutex> lock(g_missing_mutex);
            ++g_missing_globals[std::string(name, length)];
        }
    }

    lua_pushnil(state);
    return 1;
}
#endif

}  // namespace

void reset_missing_globals() {
    std::lock_guard<std::mutex> lock(g_missing_mutex);
    g_missing_globals.clear();
}

std::vector<MissingGlobal> take_missing_globals() {
    std::lock_guard<std::mutex> lock(g_missing_mutex);
    std::vector<MissingGlobal> result;
    result.reserve(g_missing_globals.size());
    for (const auto& [name, hits] : g_missing_globals) {
        result.push_back({name, hits});
    }
    g_missing_globals.clear();
    return result;
}

void register_native_bindings(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    install_lua51_compat(state);
    register_startup_bindings(state);
    register_filesystem_bindings(state);
    register_protorpc_bindings(state);
#else
    (void)state;
#endif
}

void install_missing_global_probe(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    reset_missing_globals();

    lua_pushglobaltable(state);
    if (!lua_getmetatable(state, -1)) {
        lua_newtable(state);
    }

    lua_pushcfunction(state, missing_global_index);
    lua_setfield(state, -2, "__index");
    lua_setmetatable(state, -2);
    lua_pop(state, 1);
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
