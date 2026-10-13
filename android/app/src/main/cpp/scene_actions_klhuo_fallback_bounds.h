#pragma once

#include <optional>

#include "game_scene_construction_plan.h"
#include "game_scene_type0_resource.h"
#include "scene_actions_sprite_bounds.h"

namespace nevergone::scene_actions_klhuo_fallback_bounds {

constexpr int kEmptyTextureWidth = 20;
constexpr int kEmptyTextureHeight = 20;

// The exact klhuo-1.png init branch constructs SceneActions children without a
// normal sprite. The common GameSceneObject tail then creates invisible
// `empty.png`, applies the same serialized type-0 position/rotation/scale/flip
// transform, and attaches it as the ordinary sprite used by getObjectRect() /
// isContainsRect(). The recovered imported asset metadata proves empty.png is
// an untrimmed 20x20 PNG, so its sprBoundingBox is the standard untrimmed path.
inline std::optional<scene_actions_spatial_trigger::Rect> build(
        const game_levels_scene_prefix::ObjectRecord& record) {
    const auto selected = game_scene_type0_resource::select(record.string_value);
    if (selected.kind != game_scene_type0_resource::Kind::kSceneActionPair ||
            selected.resource_name != "klhuo-1.png") {
        return std::nullopt;
    }

    const auto transform =
            game_scene_construction_plan::build_type0_sprite_transform(record);
    return scene_actions_sprite_bounds::build_untrimmed(
            transform, 0.0f, kEmptyTextureWidth, kEmptyTextureHeight);
}

}  // namespace nevergone::scene_actions_klhuo_fallback_bounds
