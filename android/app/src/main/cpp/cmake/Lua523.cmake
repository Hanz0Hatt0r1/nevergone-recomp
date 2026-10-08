include(FetchContent)

# The shipped Never Gone native library identifies its embedded interpreter as
# Lua 5.2.3. Keep this dependency exact while reconstructing the script ABI.
set(NEVERGONE_LUA_SOURCE_DIR "" CACHE PATH
    "Optional pre-extracted Lua 5.2.3 source directory (the directory containing src/lua.h)")

if(NEVERGONE_LUA_SOURCE_DIR)
    set(_lua_root "${NEVERGONE_LUA_SOURCE_DIR}")
else()
    FetchContent_Declare(
        nevergone_lua523
        URL https://www.lua.org/ftp/lua-5.2.3.tar.gz
        URL_HASH SHA256=13c2fb97961381f7d06d5b5cea55b743c163800896fd5c5e2356201d3619002d
    )
    FetchContent_GetProperties(nevergone_lua523)
    if(NOT nevergone_lua523_POPULATED)
        FetchContent_Populate(nevergone_lua523)
    endif()
    set(_lua_root "${nevergone_lua523_SOURCE_DIR}")
endif()

set(_lua_src "${_lua_root}/src")
if(NOT EXISTS "${_lua_src}/lua.h")
    message(FATAL_ERROR "Lua 5.2.3 source not found at ${_lua_src}")
endif()

add_library(lua523 STATIC
    ${_lua_src}/lapi.c
    ${_lua_src}/lcode.c
    ${_lua_src}/lctype.c
    ${_lua_src}/ldebug.c
    ${_lua_src}/ldo.c
    ${_lua_src}/ldump.c
    ${_lua_src}/lfunc.c
    ${_lua_src}/lgc.c
    ${_lua_src}/llex.c
    ${_lua_src}/lmem.c
    ${_lua_src}/lobject.c
    ${_lua_src}/lopcodes.c
    ${_lua_src}/lparser.c
    ${_lua_src}/lstate.c
    ${_lua_src}/lstring.c
    ${_lua_src}/ltable.c
    ${_lua_src}/ltm.c
    ${_lua_src}/lundump.c
    ${_lua_src}/lvm.c
    ${_lua_src}/lzio.c
    ${_lua_src}/lauxlib.c
    ${_lua_src}/lbaselib.c
    ${_lua_src}/lbitlib.c
    ${_lua_src}/lcorolib.c
    ${_lua_src}/ldblib.c
    ${_lua_src}/liolib.c
    ${_lua_src}/lmathlib.c
    ${_lua_src}/loslib.c
    ${_lua_src}/lstrlib.c
    ${_lua_src}/ltablib.c
    ${_lua_src}/loadlib.c
    ${_lua_src}/linit.c
)

target_include_directories(lua523 PUBLIC "${_lua_src}")
target_compile_features(lua523 PRIVATE c_std_99)
set_target_properties(lua523 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Lua's generic ISO-C configuration is sufficient for the current bootstrap.
# Native module dlopen support can be enabled later only if the recovered game
# actually requires external Lua C modules.
target_link_libraries(lua523 PUBLIC m)
