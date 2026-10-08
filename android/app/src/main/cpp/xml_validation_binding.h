#pragma once

struct lua_State;

namespace nevergone::lua_runtime {

void register_xml_validation_binding(lua_State* state);

}  // namespace nevergone::lua_runtime
