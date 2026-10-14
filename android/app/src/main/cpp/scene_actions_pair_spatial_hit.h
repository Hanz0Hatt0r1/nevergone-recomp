#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "scene_actions_spatial_pair_binding.h"
#include "scene_actions_spatial_trigger.h"

namespace nevergone::scene_actions_pair_spatial_hit {

enum class Status {
    kPairTriggered = 0,
    kPairNotTriggered,
    kEventOutsideSceneActionRange,
    kSourceNotFound,
    kPairNotFound,
    kSourceGeometryUnavailable,
    kPairGeometryUnavailable,
    kInvalidGeometry,
};

struct Result {
    Status status = Status::kInvalidGeometry;
    std::optional<std::size_t> source_index;
    std::optional<std::size_t> pair_index;
    std::optional<scene_actions_spatial_trigger::Rect> source_rect;
};

// Decide only whether one already-resolved SceneActions pair would receive
// triggerSceneAction() from the recovered showEventObjTip() scan. The original
// first source selection depends only on +0x108 and traversal order; unrelated
// candidate hitboxes cannot change whether this particular pair intersects the
// selected source rectangle. Therefore exact geometry is required only for the
// selected source and target pair, avoiding fabricated no-sprite state for
// still-unrecovered trimmed/animated objects.
inline Result evaluate(
        std::int32_t event_id,
        const scene_actions_spatial_pair_binding::ObjectRef& pair_ref,
        const std::vector<scene_actions_spatial_pair_binding::BoundObject>& objects) {
    Result out;
    if (event_id < scene_actions_spatial_trigger::kFirstSceneActionEventId ||
            event_id > scene_actions_spatial_trigger::kLastSceneActionEventId) {
        out.status = Status::kEventOutsideSceneActionRange;
        return out;
    }

    std::size_t source = objects.size();
    std::size_t pair = objects.size();
    for (std::size_t index = 0; index < objects.size(); ++index) {
        if (source == objects.size() && objects[index].view.event_id_108 == event_id) {
            source = index;
        }
        const auto& ref = objects[index].ref;
        if (pair == objects.size() &&
                ref.layer_index == pair_ref.layer_index &&
                ref.object_index == pair_ref.object_index) {
            pair = index;
        }
    }
    if (source == objects.size()) {
        out.status = Status::kSourceNotFound;
        return out;
    }
    if (pair == objects.size()) {
        out.status = Status::kPairNotFound;
        return out;
    }
    out.source_index = source;
    out.pair_index = pair;

    const auto& source_view = objects[source].view;
    const auto& pair_view = objects[pair].view;
    if (!source_view.sprite_bounds.has_value()) {
        out.status = Status::kSourceGeometryUnavailable;
        return out;
    }
    if (!pair_view.sprite_bounds.has_value()) {
        out.status = Status::kPairGeometryUnavailable;
        return out;
    }
    if (!scene_actions_spatial_trigger::finite_object(source_view) ||
            !scene_actions_spatial_trigger::finite_object(pair_view)) {
        out.status = Status::kInvalidGeometry;
        return out;
    }

    const auto source_rect = scene_actions_spatial_trigger::object_rect(source_view);
    if (!source_rect.has_value()) {
        out.status = Status::kInvalidGeometry;
        return out;
    }
    out.source_rect = *source_rect;

    const auto hit = scene_actions_spatial_trigger::contains_rect(pair_view, *source_rect);
    if (!hit.has_value()) {
        out.status = Status::kInvalidGeometry;
        return out;
    }
    out.status = *hit ? Status::kPairTriggered : Status::kPairNotTriggered;
    return out;
}

}  // namespace nevergone::scene_actions_pair_spatial_hit
