#pragma once

#include <string>
#include <vector>

struct lua_State;

namespace nevergone::lua_runtime {

struct MissingGlobal {
    std::string name;
    int hits = 0;
};

void register_native_bindings(lua_State* state);
void install_missing_global_probe(lua_State* state);
void reset_missing_globals();
std::vector<MissingGlobal> take_missing_globals();

}  // namespace nevergone::lua_runtime
