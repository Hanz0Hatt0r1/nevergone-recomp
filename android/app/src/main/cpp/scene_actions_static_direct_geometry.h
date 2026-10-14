#pragma once

#include <cstddef>
#include <vector>

#include "game_scene_direct_spatial_geometry.h"
#include "game_scene_render_queue.h"
#include "scene_actions_bound_object_map.h"

namespace nevergone::scene_actions_static_direct_geometry {

struct AssetSize {
    std::size_t sprite_command_index = 0;
    int width = 0;
    int height = 0;
};

enum class Status {
    kReady = 0,
    kDuplicateAsset,
    kAssetOutsideQueue,
    kInvalidAsset,
    kMissingStaticDirectAsset,
    kGeometryRejected,
};

struct Result {
    Status status = Status::kInvalidAsset;
    std::vector<scene_actions_bound_object_map::GeometryEntry> geometry;
    std::size_t animated_direct_count = 0;
    std::size_t trimmed_sprite_count = 0;
};

inline Result build(
        const game_scene_render_queue::Queue& queue,
        float camera_eye_x,
        const std::vector<AssetSize>& assets) {
    Result out;

    for (std::size_t i = 0; i < assets.size(); ++i) {
        if (assets[i].width <= 0 || assets[i].height <= 0) {
            out.status = Status::kInvalidAsset;
            return out;
        }
        if (assets[i].sprite_command_index >= queue.sprites.size()) {
            out.status = Status::kAssetOutsideQueue;
            return out;
        }
        for (std::size_t j = i + 1u; j < assets.size(); ++j) {
            if (assets[i].sprite_command_index == assets[j].sprite_command_index) {
                out.status = Status::kDuplicateAsset;
                return out;
            }
        }
    }

    for (std::size_t command_index = 0; command_index < queue.sprites.size(); ++command_index) {
        const auto& command = queue.sprites[command_index];
        if (command.player_body_transform.has_value()) continue;

        if (command.resource.kind == game_scene_type0_resource::Kind::kSpriteFrameByName) {
            ++out.trimmed_sprite_count;
            continue;
        }
        if (command.resource.kind != game_scene_type0_resource::Kind::kDirectFile ||
                !command.direct_asset_relative_path.has_value()) {
            continue;
        }
        if (command.post_init != game_scene_type0_post_init::Behavior::kNone) {
            ++out.animated_direct_count;
            continue;
        }

        const AssetSize* asset = nullptr;
        for (const auto& candidate : assets) {
            if (candidate.sprite_command_index == command_index) {
                asset = &candidate;
                break;
            }
        }
        if (asset == nullptr) {
            out.geometry.clear();
            out.status = Status::kMissingStaticDirectAsset;
            return out;
        }

        const auto entry = game_scene_direct_spatial_geometry::build(
                command,
                0.0f,
                asset->width,
                asset->height,
                camera_eye_x);
        if (!entry.has_value()) {
            out.geometry.clear();
            out.status = Status::kGeometryRejected;
            return out;
        }

        scene_actions_bound_object_map::GeometryEntry geometry;
        geometry.layer_index = entry->source_layer_index;
        geometry.object_index = entry->source_object_index;
        geometry.object_position_x = entry->object_position_x;
        geometry.sprite_bounds = entry->sprite_bounds;
        out.geometry.push_back(geometry);
    }

    out.status = Status::kReady;
    return out;
}

}  // namespace nevergone::scene_actions_static_direct_geometry
