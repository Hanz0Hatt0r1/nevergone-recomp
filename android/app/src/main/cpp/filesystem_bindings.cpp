#include "filesystem_bindings.h"

#include <filesystem>
#include <string>
#include <system_error>

#include "startup_contract.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {
namespace {

constexpr const char* kSearchDirectories[] = {
    "player",
    "player/Player01_Res",
    "player/Player01_Res/Weapons_Res",
    "player/Player01_Res/bz_res",
    "player/Player02_Res",
    "player/Player02_Res/Weapons_Res",
    "player/Player02_Res/bz_res",
    "player/Player03_Res",
    "player/Player03_Res/Weapons_Res",
    "player/actions_data/adata/p01/normal",
    "player/actions_data/adata/p02/normal",
    "player/actions_data/adata/p03/normal",
    "player/actions_data/T_adata/normal",
    "other",
    "sound",
    "sound/ENCV",
    "sound/UI_Sound",
    "sound/player01_sound",
    "sound/player02_sound",
    "sound/player03_sound",
    "gamescene",
    "gamescene/gs_actions/res",
    "gamescene/task",
    "gamescene/gs_list",
    "gamescene/scene_data_files",
    "gamescene/gs_res_image_file",
    "shader",
    "controls_ui",
    "enemy",
    "enemy/enemy_t_res",
    "enemy/eo_files",
    "enemy/enemy_ai",
    "pets",
    "pets/pets_t_res",
    "pets/pets_ai",
    "lansquenet",
    "lansquenet/l_t_res",
    "lansquenet/lansquenet_ai",
    "NPC",
    "NPC/npc_e_res",
    "weapons",
    "equips",
    "Login/ChooseHero",
    "Login/LoginScreenUI",
    "NewMap",
    "NewMap/CL_UI",
    "opengame_ui",
    "ChooseHero",
    "gamescene_ui",
    "gamescene_ui/loading_UI",
    "gamescene_ui/WH_UI",
    "gamescene_ui/WS_UI",
    "gamescene_ui/GameOver_UI",
    "gamescene_ui/CI_UI",
    "gamescene_ui/ST_UI",
    "gamescene_ui/Store_UI",
    "gamescene_ui/EndlessMooe_ui",
    "gamescene_ui/SingleLogin_UI/actions",
    "NPC/n14",
};

std::filesystem::path assets_root() {
    return std::filesystem::path(startup::config().files_dir) / "assets";
}

bool safe_relative_path(const std::string& value, std::filesystem::path* output) {
    if (value.empty()) return false;
    const std::filesystem::path input(value);
    if (input.is_absolute()) return false;

    const auto normalized = input.lexically_normal();
    for (const auto& part : normalized) {
        if (part == "..") return false;
    }
    *output = normalized;
    return true;
}

#if defined(NEVERGONE_HAS_LUA)
void push_string(lua_State* state, const std::string& value) {
    lua_pushlstring(state, value.data(), value.size());
}

int l_Lua_GetSetFilePath(lua_State* state) {
    push_string(state, writable_path());
    return 1;
}

int l_Lua_GetPathWithFileName(lua_State* state) {
    const char* name = luaL_checkstring(state, 1);
    push_string(state, name != nullptr ? resolve_resource_path(name) : std::string{});
    return 1;
}

int l_LGG_IsFileExist(lua_State* state) {
    const char* path = luaL_checkstring(state, 1);
    lua_pushboolean(state, path != nullptr && file_exists(path));
    return 1;
}

int l_Lua_CopyFile(lua_State* state) {
    const char* source = luaL_checkstring(state, 1);
    const char* destination = luaL_checkstring(state, 2);
    if (source == nullptr || destination == nullptr || !copy_file(source, destination)) {
        // The recovered scripts treat this as a command and ignore a return
        // value. Keep failures visible in Lua rather than silently succeeding.
        return luaL_error(state, "Lua_CopyFile failed");
    }
    return 0;
}

void set_global(lua_State* state, const char* name, lua_CFunction function) {
    lua_pushcfunction(state, function);
    lua_setglobal(state, name);
}
#endif

}  // namespace

std::string writable_path() {
    if (startup::config().files_dir.empty()) return {};
    std::string result = startup::config().files_dir;
    if (result.back() != '/') result.push_back('/');
    return result;
}

std::string resolve_resource_path(const std::string& name) {
    std::filesystem::path relative;
    if (!safe_relative_path(name, &relative)) return {};

    std::error_code ec;
    const auto root = assets_root();
    const auto direct = root / relative;
    if (std::filesystem::is_regular_file(direct, ec)) return direct.string();

    for (const char* directory : kSearchDirectories) {
        ec.clear();
        const auto candidate = root / directory / relative;
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate.string();
    }

    // Cocos2d-x callers often pass the returned path straight to a later file
    // open. Returning the root-relative candidate preserves that deterministic
    // path even when the file is absent, while missing files still fail later.
    return direct.string();
}

bool file_exists(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(path), ec);
}

bool copy_file(const std::string& source, const std::string& destination) {
    if (source.empty() || destination.empty()) return false;
    std::error_code ec;
    const std::filesystem::path destination_path(destination);
    if (destination_path.has_parent_path()) {
        std::filesystem::create_directories(destination_path.parent_path(), ec);
        if (ec) return false;
    }
    ec.clear();
    return std::filesystem::copy_file(
        std::filesystem::path(source),
        destination_path,
        std::filesystem::copy_options::overwrite_existing,
        ec);
}

void register_filesystem_bindings(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    set_global(state, "Lua_GetSetFilePath", l_Lua_GetSetFilePath);
    set_global(state, "Lua_GetPathWithFileName", l_Lua_GetPathWithFileName);
    set_global(state, "LGG_IsFileExist", l_LGG_IsFileExist);
    set_global(state, "Lua_CopyFile", l_Lua_CopyFile);
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
