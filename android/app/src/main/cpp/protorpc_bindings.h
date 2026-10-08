#pragma once

struct lua_State;

namespace nevergone::lua_runtime {

void register_protorpc_bindings(lua_State* state);

}  // namespace nevergone::lua_runtime
