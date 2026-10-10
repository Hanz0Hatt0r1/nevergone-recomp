#include <cassert>
#include <string>

#include "game_scene_direct_asset.h"

int main() {
    namespace asset = nevergone::game_scene_direct_asset;
    namespace resource = nevergone::game_scene_type0_resource;

    assert(asset::is_safe_basename("gktianchong.png"));
    assert(!asset::is_safe_basename(""));
    assert(!asset::is_safe_basename(".."));
    assert(!asset::is_safe_basename("../gktianchong.png"));
    assert(!asset::is_safe_basename("sub/gktianchong.png"));
    assert(!asset::is_safe_basename("sub\\gktianchong.png"));

    const resource::Selection direct{resource::Kind::kDirectFile, "gktianchong.png"};
    const auto path = asset::imported_relative_path(direct);
    assert(path.has_value());
    assert(*path == "gamescene/gs_res_image_file/gktianchong.png");

    const resource::Selection frame{resource::Kind::kSpriteFrameByName, "gkshu01.png"};
    assert(!asset::imported_relative_path(frame).has_value());

    const resource::Selection action{resource::Kind::kSceneActionPair, "klhuo-1.png"};
    assert(!asset::imported_relative_path(action).has_value());

    const resource::Selection unsafe{resource::Kind::kDirectFile, "../escape.png"};
    assert(!asset::imported_relative_path(unsafe).has_value());
    return 0;
}
