#pragma once

#include <string>

namespace nevergone::game_scene_type0_resource {

enum class Kind {
    // Most recovered type-0 visual objects resolve their serialized string
    // through CCSpriteFrameCache::spriteFrameByName() and then create a sprite
    // from the returned frame.
    kSpriteFrameByName = 0,
    // A small recovered set bypasses the frame cache and calls
    // CCSprite::create(serialized_name) directly.
    kDirectFile,
    // klhuo-1.png does not enter the ordinary sprite construction path. The
    // original creates paired SceneActionsSystem objects from formatted
    // actData names instead, so a renderer must not treat it as a static
    // sprite resource.
    kSceneActionPair,
};

struct Selection {
    Kind kind = Kind::kSpriteFrameByName;
    std::string resource_name;
};

inline Selection select(const std::string& serialized_name) {
    if (serialized_name == "klhuo-1.png") {
        return {Kind::kSceneActionPair, serialized_name};
    }
    if (serialized_name == "czyanwu1.png" ||
        serialized_name == "czyanwu3.png" ||
        serialized_name == "gktianchong.png" ||
        serialized_name == "gkyuanjing.png") {
        return {Kind::kDirectFile, serialized_name};
    }
    return {Kind::kSpriteFrameByName, serialized_name};
}

inline const char* kind_name(Kind kind) {
    switch (kind) {
        case Kind::kSpriteFrameByName: return "sprite-frame-by-name";
        case Kind::kDirectFile: return "direct-file";
        case Kind::kSceneActionPair: return "scene-action-pair";
    }
    return "unknown";
}

}  // namespace nevergone::game_scene_type0_resource
