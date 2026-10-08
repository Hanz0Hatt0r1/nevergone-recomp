#pragma once

#include <string>

struct lua_State;

namespace nevergone::lua_runtime {

void register_filesystem_bindings(lua_State* state);

// Clean-room filesystem semantics used by the Lua wrappers and tests.
std::string writable_path();
std::string resolve_resource_path(const std::string& name);
bool file_exists(const std::string& path);
bool copy_file(const std::string& source, const std::string& destination);

}  // namespace nevergone::lua_runtime
