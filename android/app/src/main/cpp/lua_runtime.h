#pragma once

#include <string>

namespace nevergone::lua_runtime {

bool available();
std::string version();
std::string smoke_test();

}  // namespace nevergone::lua_runtime
