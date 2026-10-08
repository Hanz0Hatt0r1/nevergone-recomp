#pragma once

struct lua_State;

namespace nevergone::lua_runtime {

void install_lua51_compat(lua_State* state);

}  // namespace nevergone::lua_runtime
