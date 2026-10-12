#pragma once

#include <optional>
#include <string>

#include "game_scene_type0_resource.h"

namespace nevergone::game_scene_direct_asset {

// The original application installs gamescene/gs_res_image_file as a Cocos
// search path. Direct type-0 sprite names are passed to CCSprite::create() as
// basenames, so the clean-room importer resolves them below filesDir/assets.
constexpr const char* kImportedSearchPath = "gamescene/gs_res_image_file";
constexpr const char* kRootEmptyAsset = "empty.png";

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

    // The shipped APK stores the generic transparent fallback directly at
    // assets/empty.png. AppDelegate installs the writable/imported asset root
    // before its child search directories, so CCSprite::create("empty.png")
    // resolves this root file rather than gamescene/gs_res_image_file/empty.png.
    if (selection.resource_name == kRootEmptyAsset) {
        return std::string(kRootEmptyAsset);
    }
    return std::string(kImportedSearchPath) + "/" + selection.resource_name;
}

}  // namespace nevergone::game_scene_direct_asset
