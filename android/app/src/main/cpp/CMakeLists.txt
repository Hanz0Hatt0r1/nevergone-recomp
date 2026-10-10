cmake_minimum_required(VERSION 3.22.1)
project(nevergone_recomp LANGUAGES C CXX)

add_library(nevergone_recomp SHARED
    app_delegate_state.cpp
    character_name_action_executor.cpp
    character_name_action_runtime.cpp
    character_name_assets.cpp
    character_name_assets_jni.cpp
    character_name_compositor.cpp
    character_name_input.cpp
    character_name_input_surface.cpp
    character_name_layout.cpp
    character_name_state.cpp
    character_random_name.cpp
    choose_hero_action_control_assets.cpp
    choose_hero_action_control_compositor.cpp
    choose_hero_action_control_layout.cpp
    choose_hero_action_state.cpp
    choose_hero_background_assets.cpp
    choose_hero_background_compositor.cpp
    choose_hero_background_timeline.cpp
    choose_hero_black_cloud_compositor.cpp
    choose_hero_black_cloud_timeline.cpp
    choose_hero_profile_assets.cpp
    choose_hero_profile_compositor.cpp
    choose_hero_profile_text.cpp
    choose_hero_role_focus_asset.cpp
    choose_hero_role_focus_compositor.cpp
    choose_hero_role_focus_timeline.cpp
    choose_hero_role_item_assets.cpp
    choose_hero_role_item_compositor.cpp
    choose_hero_role_item_view.cpp
    choose_hero_role_selection_state.cpp
    choose_hero_thunder_effect_compositor.cpp
    choose_hero_thunder_scheduler.cpp
    choose_hero_thunder_state.cpp
    client_callback_bridge.cpp
    created_role_transition.cpp
    created_role_transition_runtime.cpp
    filesystem_bindings.cpp
    fresh_role_compat_jni.cpp
    fresh_role_compat_state.cpp
    game_clock.cpp
    game_scene_construction_plan.cpp
    game_scene_direct_asset_bridge.cpp
    game_scene_direct_asset_store.cpp
    game_scene_direct_asset_store_bridge.cpp
    game_scene_direct_geometry.cpp
    game_scene_direct_renderer.cpp
    game_scene_direct_texture_bridge.cpp
    game_scene_direct_texture_cache.cpp
    game_scene_direct_texture_gl.cpp
    game_scene_frame_asset_bridge.cpp
    game_scene_frame_asset_store.cpp
    game_scene_frame_asset_store_bridge.cpp
    game_levels_actions_section.cpp
    game_levels_asset_probe.cpp
    game_levels_enter_transition.cpp
    game_levels_global_section.cpp
    game_levels_layer_tail.cpp
    game_levels_model.cpp
    game_levels_port_navigation.cpp
    game_levels_port_node_graph.cpp
    game_levels_port_node_section.cpp
    game_levels_runtime_state.cpp
    game_levels_scene_instance.cpp
    game_levels_scene_navigation.cpp
    game_levels_scene_prefix.cpp
    game_levels_start_scene.cpp
    hp_data_reader.cpp
    initial_ui_transition.cpp
    login_callback_payload.cpp
    login_localization_csv.cpp
    login_lua_dispatch.cpp
    login_lua_session.cpp
    lua_compat.cpp
    lua_runtime.cpp
    lua_startup_bindings.cpp
    luaxml_compat.cpp
    management_role_action_control_compositor.cpp
    management_role_selection_compositor.cpp
    management_role_selection_view.cpp
    native_binding_registry.cpp
    native_bootstrap.cpp
    offline_startup_flow.cpp
    post_scene_overlay_jni.cpp
    protorpc_bindings.cpp
    render_bridge.cpp
    role_creation_validation.cpp
    role_selection_state.cpp
    server_selection_assets.cpp
    server_selection_compositor.cpp
    server_selection_jni.cpp
    server_selection_layout.cpp
    server_selection_state.cpp
    server_selection_view.cpp
    single_login_cloud_timeline.cpp
    single_login_compositor.cpp
    single_login_light_timeline.cpp
    single_login_lightning_timeline.cpp
    single_login_scene_bridge.cpp
    single_login_sway_timeline.cpp
    single_login_thunder_bridge.cpp
    single_select_hero_compositor.cpp
    single_select_hero_confirm_compositor.cpp
    single_select_hero_confirm_input.cpp
    single_select_hero_confirm_input_surface.cpp
    single_select_hero_input.cpp
    single_select_hero_input_surface.cpp
    single_select_hero_state.cpp
    single_select_hero_transition_timeline.cpp
    splash_layer_renderer.cpp
    splash_sequence_state.cpp
    splash_timeline.cpp
    standalone_hero_save_metadata.cpp
    standalone_hero_save_probe.cpp
    startup_audio_bridge.cpp
    startup_contract.cpp
    string_validation.cpp
    string_validation_bindings.cpp
    tap_to_start_state.cpp
    xml_validation_binding.cpp
)

target_compile_features(nevergone_recomp PRIVATE cxx_std_17)

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
    target_include_directories(nevergone_recomp PRIVATE "${NEVERGONE_LUA_SOURCE_DIR}")
    target_link_libraries(nevergone_recomp PRIVATE nevergone_lua)
else()
    message(WARNING "Verified Lua 5.2.3 sources not found; native Lua runtime disabled")
endif()

find_library(log-lib log)
find_library(android-lib android)
find_library(gles-lib GLESv2)
find_library(z-lib z)
target_link_libraries(nevergone_recomp PRIVATE ${log-lib} ${android-lib} ${gles-lib} ${z-lib})