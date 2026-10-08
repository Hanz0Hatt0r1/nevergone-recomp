cmake_minimum_required(VERSION 3.22.1)
project(nevergone_recomp LANGUAGES C CXX)

add_library(nevergone_recomp SHARED
    app_delegate_state.cpp
    choose_hero_background_assets.cpp
    client_callback_bridge.cpp
    filesystem_bindings.cpp
    game_clock.cpp
    game_levels_asset_probe.cpp
    game_levels_scene_prefix.cpp
    hp_data_reader.cpp
    initial_ui_transition.cpp
    lua_compat.cpp
    lua_runtime.cpp
    lua_startup_bindings.cpp
    luaxml_compat.cpp
    native_binding_registry.cpp
    native_bootstrap.cpp
    offline_startup_flow.cpp
    protorpc_bindings.cpp
    render_bridge.cpp
    single_login_cloud_timeline.cpp
    single_login_compositor.cpp
    single_login_light_timeline.cpp
    single_login_lightning_timeline.cpp
    single_login_scene_bridge.cpp
    single_login_sway_timeline.cpp
    single_login_thunder_bridge.cpp
    single_select_hero_compositor.cpp
    splash_layer_renderer.cpp
    splash_sequence_state.cpp
    splash_timeline.cpp
    standalone_hero_save_probe.cpp
    startup_audio_bridge.cpp
    startup_contract.cpp
    string_validation_bindings.cpp
    tap_to_start_state.cpp
    xml_validation_binding.cpp
)

target_compile_features(nevergone_recomp PRIVATE cxx_std_17)

# Keep the final shared object loadable on Android devices that use 16 KiB
# memory pages. NDK r28 already does this for arm64, but the armeabi-v7a link
# still defaults to 4 KiB PT_LOAD alignment unless the maximum page size is
# stated explicitly.
if(ANDROID)
    target_link_options(nevergone_recomp PRIVATE "-Wl,-z,max-page-size=16384")
endif()

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
    GLESv2
    log
)
