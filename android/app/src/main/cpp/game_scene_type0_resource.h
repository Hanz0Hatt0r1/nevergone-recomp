#pragma once

#include <string>

namespace nevergone::game_scene_type0_resource {

enum class Kind {
    kSpriteFrameByName = 0,
    kDirectFile,
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
