cmake_minimum_required(VERSION 3.22.1)
project(nevergone_recomp LANGUAGES C CXX)

add_library(nevergone_recomp SHARED
    lua_runtime.cpp
    lua_startup_bindings.cpp
    native_binding_registry.cpp
    native_bootstrap.cpp
    startup_contract.cpp
)

target_compile_features(nevergone_recomp PRIVATE cxx_std_17)

set(
    NEVERGONE_LUA_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../third_party/_local/lua-5.2.3/src"
    CACHE PATH
    "Path to the verified Lua 5.2.3 src directory"
)

if(EXISTS "${NEVERGONE_LUA_SOURCE_DIR}/lua.h")
    set(LUA_SOURCES
        lapi.c
        lauxlib.c
        lbaselib.c
        lbitlib.c
        lcode.c
        lcorolib.c
        lctype.c
        ldblib.c
        ldebug.c
        ldo.c
        ldump.c
        lfunc.c
        lgc.c
        linit.c
        liolib.c
        llex.c
        lmathlib.c
        lmem.c
        loadlib.c
        lobject.c
        lopcodes.c
        loslib.c
        lparser.c
        lstate.c
        lstring.c
        lstrlib.c
        ltable.c
        ltablib.c
        ltm.c
        lundump.c
        lvm.c
        lzio.c
    )
    list(TRANSFORM LUA_SOURCES PREPEND "${NEVERGONE_LUA_SOURCE_DIR}/")

    add_library(nevergone_lua STATIC ${LUA_SOURCES})
    target_include_directories(nevergone_lua PUBLIC "${NEVERGONE_LUA_SOURCE_DIR}")
    target_compile_definitions(nevergone_recomp PRIVATE NEVERGONE_HAS_LUA=1)
    target_link_libraries(nevergone_recomp PRIVATE nevergone_lua dl m)
else()
    message(STATUS "Lua 5.2.3 source not found; building bootstrap without Lua runtime")
endif()

target_link_libraries(nevergone_recomp PRIVATE
    android
    log
)
