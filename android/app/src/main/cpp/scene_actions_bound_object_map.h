#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "game_scene_construction_plan.h"
#include "scene_actions_bound_object_builder.h"
#include "scene_actions_spatial_trigger.h"

namespace nevergone::scene_actions_bound_object_map {

struct GeometryEntry {
    std::size_t layer_index = 0;
    std::size_t object_index = 0;
    float object_position_x = 0.0f;
    std::optional<scene_actions_spatial_trigger::Rect> sprite_bounds;
};

enum class Status {
    kReady = 0,
    kDuplicateGeometry,
    kGeometryOutsideScene,
    kInvalidGeometry,
};

struct Result {
    Status status = Status::kInvalidGeometry;
    std::vector<scene_actions_spatial_pair_binding::BoundObject> objects;
};

inline bool same_ref(
        const GeometryEntry& geometry,
        std::size_t layer_index,
        std::size_t object_index) {
    return geometry.layer_index == layer_index && geometry.object_index == object_index;
}

// Compose the complete object array consumed by showEventObjTip() from the
// retained ScenePlan plus geometry supplied by the live renderer boundary.
// ScenePlan traversal order is preserved exactly. Missing geometry is valid and
// models GameSceneObject +0x10c == nullptr; duplicate or orphan geometry fails
// closed so a stale renderer snapshot cannot be paired with a different scene.
inline Result build(
        const game_scene_construction_plan::ScenePlan& scene,
        const std::vector<GeometryEntry>& geometry) {
    Result out;

    for (std::size_t i = 0; i < geometry.size(); ++i) {
        const auto& item = geometry[i];
        if (!std::isfinite(item.object_position_x) ||
                (item.sprite_bounds.has_value() &&
                 !scene_actions_spatial_trigger::finite_rect(*item.sprite_bounds))) {
            out.status = Status::kInvalidGeometry;
            return out;
        }
        for (std::size_t j = i + 1u; j < geometry.size(); ++j) {
            if (item.layer_index == geometry[j].layer_index &&
                    item.object_index == geometry[j].object_index) {
                out.status = Status::kDuplicateGeometry;
                return out;
            }
        }
    }

    std::vector<bool> consumed(geometry.size(), false);
    out.objects.reserve(scene.object_count);
    for (const auto& layer : scene.layers) {
        for (const auto& object : layer.objects) {
            scene_actions_bound_object_builder::GeometryInput input;
            for (std::size_t index = 0; index < geometry.size(); ++index) {
                if (!same_ref(
                            geometry[index],
                            layer.source_layer_index,
                            object.source_object_index)) {
                    continue;
                }
                input.object_position_x = geometry[index].object_position_x;
                input.sprite_bounds = geometry[index].sprite_bounds;
                consumed[index] = true;
                break;
            }
            out.objects.push_back(scene_actions_bound_object_builder::build(
                    layer.source_layer_index,
                    object.source_object_index,
                    object.record,
                    input));
        }
    }

    for (const bool used : consumed) {
        if (!used) {
            out.objects.clear();
            out.status = Status::kGeometryOutsideScene;
            return out;
        }
    }

    out.status = Status::kReady;
    return out;
}

}  // namespace nevergone::scene_actions_bound_object_map
