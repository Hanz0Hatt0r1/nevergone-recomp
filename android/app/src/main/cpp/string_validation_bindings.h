#pragma once

struct lua_State;

namespace nevergone::lua_runtime {

void register_string_validation_bindings(lua_State* state);

}  // namespace nevergone::lua_runtime
