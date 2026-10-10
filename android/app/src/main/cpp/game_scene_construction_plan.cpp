#include "game_scene_construction_plan.h"

namespace nevergone::game_scene_construction_plan {

const char* construction_kind_name(ObjectConstructionKind kind) {
    switch (kind) {
        case ObjectConstructionKind::kUnresolved: return "unresolved";
        case ObjectConstructionKind::kType0SpriteBacked: return "type0-sprite-backed";
        case ObjectConstructionKind::kType0SceneActionPair: return "type0-scene-action-pair";
    }
    return "unknown";
}

}  // namespace nevergone::game_scene_construction_plan
