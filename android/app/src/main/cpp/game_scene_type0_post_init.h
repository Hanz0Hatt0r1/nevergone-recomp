#pragma once

#include <string>

namespace nevergone::game_scene_type0_post_init {

enum class Behavior {
    kNone = 0,
    // Recovered GameSceneObject::initWithData() gkshu01..08 branch.
    // The original applies randomized skew and runs the resulting sequence forever.
    kRandomSkewLoop,
    // Recovered GameSceneObject::initWithData() gkdeng01..07 branch.
    // The original applies randomized delay/fade timing and repeats it forever.
    kRandomFadeLoop,
};

inline Behavior classify(const std::string& serialized_name) {
    if (serialized_name == "gkshu01.png" ||
        serialized_name == "gkshu02.png" ||
        serialized_name == "gkshu03.png" ||
        serialized_name == "gkshu04.png" ||
        serialized_name == "gkshu05.png" ||
        serialized_name == "gkshu06.png" ||
        serialized_name == "gkshu07.png" ||
        serialized_name == "gkshu08.png") {
        return Behavior::kRandomSkewLoop;
    }

    if (serialized_name == "gkdeng01.png" ||
        serialized_name == "gkdeng02.png" ||
        serialized_name == "gkdeng03.png" ||
        serialized_name == "gkdeng04.png" ||
        serialized_name == "gkdeng05.png" ||
        serialized_name == "gkdeng06.png" ||
        serialized_name == "gkdeng07.png") {
        return Behavior::kRandomFadeLoop;
    }

    return Behavior::kNone;
}

inline const char* behavior_name(Behavior behavior) {
    switch (behavior) {
        case Behavior::kNone: return "none";
        case Behavior::kRandomSkewLoop: return "random-skew-loop";
        case Behavior::kRandomFadeLoop: return "random-fade-loop";
    }
    return "unknown";
}

}  // namespace nevergone::game_scene_type0_post_init
