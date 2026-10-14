#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include "game_scene_construction_plan.h"
#include "game_scene_layer_object_parent_x.h"
#include "scene_actions_bound_object_map.h"
#include "scene_actions_klhuo_fallback_bounds.h"

namespace nevergone::scene_actions_klhuo_bound_geometry {

enum class Status {
    kReady = 0,
    kInvalidMoveInput,
    kInvalidLayerParentX,
    kInvalidFallbackBounds,
};

struct Result {
    Status status = Status::kInvalidMoveInput;
    std::vector<scene_actions_bound_object_map::GeometryEntry> geometry;
};

// Derive only the exact klhuo-1.png GameSceneObject geometry whose native
// spatial path is now fully recovered. The hidden empty.png sprite supplies the
// local sprBoundingBox(); GameSceneClassLayer::move() supplies the parent node X
// added by getObjectRect()/isContainsRect(). Other objects remain the renderer's
// responsibility so animated skew/trimmed-frame details are not guessed here.
inline Result build(
        const game_scene_construction_plan::ScenePlan& scene,
        float layer_move_input_x) {
    Result out;
    if (!std::isfinite(layer_move_input_x)) return out;

    out.geometry.reserve(scene.layers.size());
    for (const auto& layer : scene.layers) {
        for (const auto& object : layer.objects) {
            if (object.construction_kind !=
                    game_scene_construction_plan::ObjectConstructionKind::kType0SceneActionPair) {
                continue;
            }
            if (!object.type0_resource.has_value() ||
                    object.type0_resource->kind !=
                        game_scene_type0_resource::Kind::kSceneActionPair ||
                    object.type0_resource->resource_name != "klhuo-1.png") {
                continue;
            }

            const auto parent_x = game_scene_layer_object_parent_x::from_move_input(
                    layer.z_index, layer.first_float, layer_move_input_x);
            if (!parent_x.has_value()) {
                out.geometry.clear();
                out.status = Status::kInvalidLayerParentX;
                return out;
            }
            const auto bounds = scene_actions_klhuo_fallback_bounds::build(object.record);
            if (!bounds.has_value()) {
                out.geometry.clear();
                out.status = Status::kInvalidFallbackBounds;
                return out;
            }

            scene_actions_bound_object_map::GeometryEntry entry;
            entry.layer_index = layer.source_layer_index;
            entry.object_index = object.source_object_index;
            entry.object_position_x = *parent_x;
            entry.sprite_bounds = *bounds;
            out.geometry.push_back(entry);
        }
    }

    out.status = Status::kReady;
    return out;
}

}  // namespace nevergone::scene_actions_klhuo_bound_geometry
