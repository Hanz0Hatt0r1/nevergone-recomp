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
    assert(*path == "gktianchong.png");

    const resource::Selection empty{resource::Kind::kDirectFile, "empty.png"};
    const auto empty_path = asset::imported_relative_path(empty);
    assert(empty_path.has_value());
    assert(*empty_path == "empty.png");

    const auto gkanniu = resource::select("gkanniu2.png");
    assert(gkanniu.kind == resource::Kind::kDirectFile);
    assert(gkanniu.resource_name == "empty.png");
    const auto gkanniu_path = asset::imported_relative_path(gkanniu);
    assert(gkanniu_path.has_value());
    assert(*gkanniu_path == "empty.png");

    const resource::Selection frame{resource::Kind::kSpriteFrameByName, "gkshu01.png"};
    assert(!asset::imported_relative_path(frame).has_value());

    const resource::Selection action{resource::Kind::kSceneActionPair, "klhuo-1.png"};
    assert(!asset::imported_relative_path(action).has_value());

    const resource::Selection unsafe{resource::Kind::kDirectFile, "../escape.png"};
    assert(!asset::imported_relative_path(unsafe).has_value());
    return 0;
}
