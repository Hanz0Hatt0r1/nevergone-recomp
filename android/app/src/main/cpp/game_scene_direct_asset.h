#pragma once

#include <optional>
#include <string>

#include "game_scene_type0_resource.h"

namespace nevergone::game_scene_direct_asset {

// The original application installs gamescene/gs_res_image_file as a Cocos
// search path. Direct type-0 sprite names are passed to CCSprite::create() as
// basenames, so the clean-room importer resolves them below filesDir/assets.
constexpr const char* kImportedSearchPath = "gamescene/gs_res_image_file";

inline bool is_safe_basename(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    return name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos &&
           name.find("..") == std::string::npos;
}

inline std::optional<std::string> imported_relative_path(
        const game_scene_type0_resource::Selection& selection) {
    if (selection.kind != game_scene_type0_resource::Kind::kDirectFile ||
        !is_safe_basename(selection.resource_name)) {
        return std::nullopt;
    }
    return std::string(kImportedSearchPath) + "/" + selection.resource_name;
}

}  // namespace nevergone::game_scene_direct_asset
