#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "game_levels_scene_instance.h"
#include "game_scene_type0_resource.h"

namespace nevergone::game_scene_construction_plan {

// GameScene::loadingTex() in the original 1.0.9 ARMv7 binary requests
// GameSceneData::GetSceneLayerDataWithZ() for indexes 0 through 10.
constexpr std::size_t kLoadingTexLayerSlotCount = 11u;

enum class ObjectConstructionKind {
    kUnresolved = 0,
    // GameSceneObject::initWithData() type 0 paths that create a CCSprite.
    kType0SpriteBacked,
    // Exact type-0 klhuo-1.png branch. It creates paired SceneActionsSystem
    // objects and does not create the ordinary static sprite at this boundary.
    kType0SceneActionPair,
};

// These members are named semantically only at this renderer-facing boundary.
// In the original 1.0.9 ARMv7 GameSceneObject::initWithData() sprite paths,
// the corresponding source fields are passed directly to CCSprite::setPosition,
// setRotation, setScaleX, setScaleY and setFlipX, then the sprite is attached
// through CCNode::addChild(sprite, z_order).
struct Type0SpriteTransform {
    float position_x = 0.0f;
    float position_y = 0.0f;
    float rotation = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    bool flip_x = false;
    std::int32_t child_z_order = 0;
};

struct ObjectPlan {
    std::size_t source_object_index = 0;
    std::int32_t type_code = 0;
    ObjectConstructionKind construction_kind = ObjectConstructionKind::kUnresolved;
    std::optional<game_scene_type0_resource::Selection> type0_resource;
    std::optional<Type0SpriteTransform> type0_sprite_transform;
    // Keep the complete evidence-backed serialized record available to later
    // project-owned adapters without renaming still-unresolved fields in the
    // binary parser itself.
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

inline ObjectConstructionKind classify_object(std::int32_t type_code) {
    if (type_code == 0) return ObjectConstructionKind::kType0SpriteBacked;
    return ObjectConstructionKind::kUnresolved;
}

inline Type0SpriteTransform build_type0_sprite_transform(
        const game_levels_scene_prefix::ObjectRecord& record) {
    Type0SpriteTransform transform;
    transform.position_x = record.first_point_x;
    transform.position_y = record.first_point_y;
    transform.rotation = record.middle_float;
    transform.scale_x = record.second_point_x;
    transform.scale_y = record.second_point_y;
    transform.flip_x = record.first_bool;
    transform.child_z_order = record.trailing_i32;
    return transform;
}

// This is intentionally a pure project-owned value transformation. Keep it
// inline so runtime consumers that retain a ScenePlan do not acquire an
// otherwise artificial host-link dependency on the diagnostic implementation
// unit.
inline ScenePlan build(const game_levels_scene_instance::SceneInstance& scene) {
    ScenePlan plan;
    plan.source_scene_index = scene.source_scene_index;
    plan.guid = scene.guid;
    plan.first_point_x = scene.first_point_x;
    plan.first_point_y = scene.first_point_y;

    const std::size_t layer_count = std::min(scene.layers.size(), kLoadingTexLayerSlotCount);
    plan.layers.reserve(layer_count);
    plan.ignored_source_layer_count = scene.layers.size() - layer_count;

    for (std::size_t z = 0; z < layer_count; ++z) {
        const auto& source_layer = scene.layers[z];
        LayerPlan layer;
        layer.z_index = z;
        layer.source_layer_index = source_layer.source_layer_index;
        layer.first_float = source_layer.first_float;
        layer.top_border_points = source_layer.top_border_points;
        layer.bottom_border_points = source_layer.bottom_border_points;
        layer.objects.reserve(source_layer.objects.size());

        for (std::size_t object_index = 0; object_index < source_layer.objects.size(); ++object_index) {
            const auto& source_object = source_layer.objects[object_index];
            ObjectPlan object;
            object.source_object_index = object_index;
            object.type_code = source_object.first_i32;
            object.construction_kind = classify_object(object.type_code);
            if (object.type_code == 0) {
                object.type0_resource = game_scene_type0_resource::select(source_object.string_value);
                if (object.type0_resource->kind == game_scene_type0_resource::Kind::kSceneActionPair) {
                    object.construction_kind = ObjectConstructionKind::kType0SceneActionPair;
                } else {
                    object.type0_sprite_transform = build_type0_sprite_transform(source_object);
                }
            }
            object.record = source_object;
            layer.objects.push_back(std::move(object));
            ++plan.object_count;
        }

        plan.layers.push_back(std::move(layer));
    }

    return plan;
}

const char* construction_kind_name(ObjectConstructionKind kind);

}  // namespace nevergone::game_scene_construction_plan
