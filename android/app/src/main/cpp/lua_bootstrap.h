#pragma once

#include <string>

struct lua_State;

namespace nevergone {

class LuaRuntime {
public:
    LuaRuntime(std::string script_root, std::string device_id);
    ~LuaRuntime();

    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;

    bool initialize();
    bool ready() const;
    std::string smoke_test();
    const std::string& last_error() const;
    const std::string& script_root() const;
    const std::string& device_id() const;

private:
    lua_State* state_ = nullptr;
    std::string script_root_;
    std::string device_id_;
    std::string last_error_;
};

}  // namespace nevergone
