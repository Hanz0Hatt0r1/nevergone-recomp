#pragma once

struct lua_State;

namespace nevergone::lua_runtime {

void install_luaxml_compat(lua_State* state);

}  // namespace nevergone::lua_runtime
