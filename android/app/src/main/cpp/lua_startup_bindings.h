#pragma once

#include <string>

struct lua_State;

namespace nevergone::lua_runtime {

#if defined(NEVERGONE_HAS_LUA)
void register_startup_bindings(lua_State* state);
bool execute_module(lua_State* state, const std::string& module_name, std::string* error);
#endif

std::string startup_execution_report();

}  // namespace nevergone::lua_runtime
