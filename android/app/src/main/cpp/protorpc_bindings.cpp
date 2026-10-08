#include "protorpc_bindings.h"

#include <new>
#include <string>
#include <vector>

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {

#if defined(NEVERGONE_HAS_LUA)
namespace {

constexpr const char* kProtoRpcMetatable = "NeverGone.ProtoRPC";

struct ProtoRpcState {
    std::string id;
    std::string proto_root;
    std::string rpc_id;
    std::string session_id;
    std::vector<std::string> proto_files;
    bool closed = false;
    bool released = false;
};

ProtoRpcState* check_rpc(lua_State* state, int index = 1) {
    return static_cast<ProtoRpcState*>(luaL_checkudata(state, index, kProtoRpcMetatable));
}

int l_new(lua_State* state) {
    void* storage = lua_newuserdata(state, sizeof(ProtoRpcState));
    new (storage) ProtoRpcState();
    luaL_getmetatable(state, kProtoRpcMetatable);
    lua_setmetatable(state, -2);
    return 1;
}

int l_gc(lua_State* state) {
    ProtoRpcState* rpc = check_rpc(state);
    rpc->~ProtoRpcState();
    return 0;
}

int l_SetID(lua_State* state) {
    ProtoRpcState* rpc = check_rpc(state);
    rpc->id = luaL_optstring(state, 2, "");
    return 0;
}

int l_SetProtoFileRootDir(lua_State* state) {
    ProtoRpcState* rpc = check_rpc(state);
    rpc->proto_root = luaL_optstring(state, 2, "");
    return 0;
}

int l_ImportProtoFile(lua_State* state) {
    ProtoRpcState* rpc = check_rpc(state);
    const char* name = luaL_checkstring(state, 2);
    if (name != nullptr && *name != '\0') {
        rpc->proto_files.emplace_back(name);
    }
    lua_pushboolean(state, 1);
    return 1;
}

int l_CheckConnection(lua_State* state) {
    check_rpc(state);
    lua_pushboolean(state, 0);
    return 1;
}

int l_StartRPC(lua_State* state) {
    check_rpc(state);
    // Networking is intentionally not emulated. Returning false preserves the
    // original caller contract without pretending a server connection exists.
    lua_pushboolean(state, 0);
    return 1;
}

int l_NewRequest(lua_State* state) {
    check_rpc(state);
    lua_pushnil(state);
    return 1;
}

int l_NewMessage(lua_State* state) {
    check_rpc(state);
    lua_pushnil(state);
    return 1;
}

int l_DeleteMessage(lua_State* state) {
    check_rpc(state);
    return 0;
}

int l_CallMethod(lua_State* state) {
    check_rpc(state);
    lua_pushnil(state);
    return 1;
}

int l_Close(lua_State* state) {
    ProtoRpcState* rpc = check_rpc(state);
    rpc->closed = true;
    return 0;
}

int l_release(lua_State* state) {
    ProtoRpcState* rpc = check_rpc(state);
    rpc->released = true;
    return 0;
}

int l_SetRpcID(lua_State* state) {
    ProtoRpcState* rpc = check_rpc(state);
    rpc->rpc_id = luaL_optstring(state, 2, "");
    return 0;
}

int l_SetRpcSessionID(lua_State* state) {
    ProtoRpcState* rpc = check_rpc(state);
    rpc->session_id = luaL_optstring(state, 2, "");
    return 0;
}

int l_CleanStackMsg(lua_State* state) {
    check_rpc(state);
    return 0;
}

void set_method(lua_State* state, const char* name, lua_CFunction function) {
    lua_pushcfunction(state, function);
    lua_setfield(state, -2, name);
}

}  // namespace
#endif

void register_protorpc_bindings(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    if (luaL_newmetatable(state, kProtoRpcMetatable)) {
        lua_pushvalue(state, -1);
        lua_setfield(state, -2, "__index");
        set_method(state, "__gc", l_gc);
        set_method(state, "SetID", l_SetID);
        set_method(state, "SetProtoFileRootDir", l_SetProtoFileRootDir);
        set_method(state, "ImportProtoFile", l_ImportProtoFile);
        set_method(state, "CheckConnection", l_CheckConnection);
        set_method(state, "StartRPC", l_StartRPC);
        set_method(state, "NewRequest", l_NewRequest);
        set_method(state, "NewMessage", l_NewMessage);
        set_method(state, "DeleteMessage", l_DeleteMessage);
        set_method(state, "CallMethod", l_CallMethod);
        set_method(state, "Close", l_Close);
        set_method(state, "release", l_release);
        set_method(state, "SetRpcID", l_SetRpcID);
        set_method(state, "SetRpcSessionID", l_SetRpcSessionID);
        set_method(state, "CleanStackMsg", l_CleanStackMsg);
    }
    lua_pop(state, 1);

    lua_newtable(state);
    set_method(state, "new", l_new);
    lua_setglobal(state, "ProtoRPC");
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
