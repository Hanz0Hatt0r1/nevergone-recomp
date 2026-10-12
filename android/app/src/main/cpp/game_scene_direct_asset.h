#pragma once

#include <optional>
#include <string>

#include "game_scene_type0_resource.h"

namespace nevergone::game_scene_direct_asset {

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

    // Type-0 direct resources are passed to CCSprite::create() as logical
    // basenames. Do not pin them to gamescene/gs_res_image_file here: the
    // original CCFileUtils lookup checks the writable root and every recovered
    // AddAllSearchPath entry in order. Android staging reproduces that lookup
    // against the user-owned imported asset tree.
    return selection.resource_name;
}

}  // namespace nevergone::game_scene_direct_asset
