#pragma once

#include <vector>

#include "game_scene_construction_plan.h"
#include "game_scene_render_queue.h"
#include "scene_actions_bound_object_map.h"
#include "scene_actions_klhuo_bound_geometry.h"
#include "scene_actions_static_direct_store_geometry.h"

namespace nevergone::scene_actions_current_geometry_map {

enum class Status {
    kReady = 0,
    kSceneMismatch,
    kStaticDirectUnavailable,
    kKlhuoGeometryRejected,
    kBoundMapRejected,
};

struct Result {
    Status status = Status::kSceneMismatch;
    std::vector<scene_actions_spatial_pair_binding::BoundObject> objects;
    std::size_t animated_direct_count = 0;
    std::size_t trimmed_sprite_count = 0;
};

// Compose the best evidence-backed current ObjectView array from one retained
// ScenePlan/render queue pair. Exact klhuo fallback geometry and validated
// static direct-file geometry are filled automatically. Animated direct and
// trimmed sprite-frame objects remain present in traversal order with no bounds;
// pair-only spatial evaluation can therefore fail closed only when one of those
// unresolved objects is the selected event source.
inline Result build(
        const game_scene_construction_plan::ScenePlan& scene,
        const game_scene_render_queue::Queue& queue,
        float camera_eye_x) {
    Result out;
    if (scene.source_scene_index != queue.source_scene_index || scene.guid != queue.guid) {
        return out;
    }

    const auto direct = scene_actions_static_direct_store_geometry::capture(
            queue, camera_eye_x);
    if (direct.status != scene_actions_static_direct_store_geometry::Status::kReady) {
        out.status = Status::kStaticDirectUnavailable;
        return out;
    }
    out.animated_direct_count = direct.collector.animated_direct_count;
    out.trimmed_sprite_count = direct.collector.trimmed_sprite_count;

    const auto klhuo = scene_actions_klhuo_bound_geometry::build(scene, camera_eye_x);
    if (klhuo.status != scene_actions_klhuo_bound_geometry::Status::kReady) {
        out.status = Status::kKlhuoGeometryRejected;
        return out;
    }

    std::vector<scene_actions_bound_object_map::GeometryEntry> geometry;
    geometry.reserve(direct.collector.geometry.size() + klhuo.geometry.size());
    geometry.insert(
            geometry.end(),
            direct.collector.geometry.begin(),
            direct.collector.geometry.end());
    geometry.insert(geometry.end(), klhuo.geometry.begin(), klhuo.geometry.end());

    const auto mapped = scene_actions_bound_object_map::build(scene, geometry);
    if (mapped.status != scene_actions_bound_object_map::Status::kReady) {
        out.status = Status::kBoundMapRejected;
        return out;
    }

    out.objects = mapped.objects;
    out.status = Status::kReady;
    return out;
}

}  // namespace nevergone::scene_actions_current_geometry_map
