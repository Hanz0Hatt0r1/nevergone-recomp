#pragma once

#include <string>

namespace nevergone::game_scene_type0_resource {

enum class Kind {
    // Default recovered path: serialized name -> spriteFrameByName() ->
    // createWithSpriteFrame().
    kSpriteFrameByName = 0,
    // Recovered exceptions that call CCSprite::create(file_name) directly.
    kDirectFile,
    // klhuo-1.png creates paired SceneActionsSystem objects from formatted
    // actData names instead of constructing the ordinary static sprite here.
    kSceneActionPair,
};

struct Selection {
    Kind kind = Kind::kSpriteFrameByName;
    std::string resource_name;
    // True for ordinary resources. The exact gkanniu2 branch replaces the
    // cached sprite with empty.png and immediately calls setVisible(false).
    bool initially_visible = true;
};

inline Selection select(const std::string& serialized_name) {
    if (serialized_name == "klhuo-1.png") {
        return {Kind::kSceneActionPair, serialized_name};
    }
    if (serialized_name == "gkanniu2.png") {
        // GameSceneObject::initWithData() first creates the ordinary cached
        // sprite, then replaces the live pointer with CCSprite::create(
        // "empty.png") and dispatches CCSprite::setVisible(false).
        return {Kind::kDirectFile, "empty.png", false};
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
