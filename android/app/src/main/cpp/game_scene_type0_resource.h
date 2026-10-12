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
    // Visibility immediately after the recovered GameSceneObject::initWithData()
    // resource-construction path completes. Some ordinary sprite-frame-backed
    // resources are explicitly hidden by the original after construction.
    bool initially_visible = true;
};

inline bool is_hidden_after_init(const std::string& serialized_name) {
    // Recovered 1.0.9 ARMv7 initWithData() branch: these resources are created
    // through spriteFrameByName()/createWithSpriteFrame(), then immediately
    // receive setVisible(false).
    return serialized_name == "klhuoguang-6.png" ||
           serialized_name == "klhuoguang-8.png" ||
           serialized_name == "kldimianhuoguang-1.png" ||
           serialized_name == "kldimianhuoguang-2.png" ||
           serialized_name == "klqianjingkanshouguang.png" ||
           serialized_name == "klhuoguang-1.png" ||
           serialized_name == "gjbeijing02.png" ||
           serialized_name == "gjzhongjing04.png";
}

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
    return {
        Kind::kSpriteFrameByName,
        serialized_name,
        !is_hidden_after_init(serialized_name),
    };
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
