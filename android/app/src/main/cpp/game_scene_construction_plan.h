#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "game_levels_scene_instance.h"

namespace nevergone::game_scene_construction_plan {

// GameScene::loadingTex() in the original 1.0.9 ARMv7 binary requests
// GameSceneData::GetSceneLayerDataWithZ() for indexes 0 through 10.
constexpr std::size_t kLoadingTexLayerSlotCount = 11u;

enum class ObjectConstructionKind {
    kUnresolved = 0,
    // GameSceneObject::initWithData() enters the sprite construction branch
    // when GameSceneLayerObjectData + 0x14 (the parsed leading int32) is zero.
    kType0SpriteBacked,
};

struct ObjectPlan {
    std::size_t source_object_index = 0;
    std::int32_t type_code = 0;
    ObjectConstructionKind construction_kind = ObjectConstructionKind::kUnresolved;
    // Keep the complete evidence-backed serialized record available to later
    // project-owned adapters without assigning speculative field semantics.
    game_levels_scene_prefix::ObjectRecord record;
};

struct LayerPlan {
    // loadingTex uses the CCArray index as the layer Z selector. This is not
    // derived from LayerInstance::first_float.
    std::size_t z_index = 0;
    std::size_t source_layer_index = 0;
    float first_float = 0.0f;
    std::vector<ObjectPlan> objects;
    std::vector<game_levels_layer_tail::BorderPoint> top_border_points;
    std::vector<game_levels_layer_tail::BorderPoint> bottom_border_points;
};

struct ScenePlan {
    std::size_t source_scene_index = 0;
    std::string guid;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    std::vector<LayerPlan> layers;
    std::size_t object_count = 0;
    std::size_t ignored_source_layer_count = 0;
};

ObjectConstructionKind classify_object(std::int32_t type_code);
ScenePlan build(const game_levels_scene_instance::SceneInstance& scene);
const char* construction_kind_name(ObjectConstructionKind kind);

}  // namespace nevergone::game_scene_construction_plan
