#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "game_scene_construction_plan.h"
#include "game_scene_direct_asset.h"
#include "me_actions_initial_sprite_transform.h"

namespace nevergone::game_scene_render_queue {

struct EnemyActionHitPointPosition {
    std::size_t enemy_slot = 0;
    std::int32_t action_frame_index = 0;
    std::size_t action_data_index = 0;
    std::size_t frame_ordinal = 0;
    // Coordinates in EnemyActionsSystem space; object/world translation and
    // sprite bounds are resolved by later combat integration.
    float system_local_x = 0.0f;
    float system_local_y = 0.0f;
};

struct SpriteCommand {
    std::size_t layer_z_index = 0;
    std::size_t source_layer_index = 0;
    std::size_t source_object_index = 0;
    game_scene_type0_resource::Selection resource;
    game_scene_type0_post_init::Behavior post_init =
            game_scene_type0_post_init::Behavior::kNone;
    std::optional<std::string> direct_asset_relative_path;
    game_scene_construction_plan::Type0SpriteTransform transform;
    float layer_camera_factor = 0.0f;
    std::optional<me_actions_initial_sprite_transform::Plan> player_body_transform;
    std::optional<std::string> player_body_atlas_plist;
    std::optional<me_actions_initial_sprite_transform::Plan> action_frame_transform;
    std::optional<std::string> action_frame_atlas_plist;
    std::uint8_t action_frame_opacity = 255u;
    int player_career = 0;
    int player_clothes_id = -1;
    std::string player_name;
};

// Runtime handoff for the exact type-0 klhuo-1.png branch. The original does
// not create a normal CCSprite for this object; it constructs two
// SceneActionsSystem instances. Keep the complete serialized source record so
// later SceneActions playback can consume only fields backed by evidence rather
// than fabricating a sprite transform at this boundary.
struct SceneActionPairCommand {
    std::size_t layer_z_index = 0;
    std::size_t source_layer_index = 0;
    std::size_t source_object_index = 0;
    game_scene_type0_resource::Selection resource;
    float layer_camera_factor = 0.0f;
    game_levels_scene_prefix::ObjectRecord record;
};

struct Queue {
    std::size_t source_scene_index = 0;
    std::string guid;
    std::vector<SpriteCommand> sprites;
    std::vector<SceneActionPairCommand> scene_action_pairs;
    std::vector<EnemyActionHitPointPosition> enemy_action_hitpoints;
    std::size_t sprite_frame_lookup_count = 0;
    std::size_t direct_file_count = 0;
    std::size_t scene_action_pair_count = 0;
    std::size_t unresolved_object_count = 0;
    std::size_t tutorial_enemy_action_sprite_count = 0;
};

inline Queue build(const game_scene_construction_plan::ScenePlan& plan) {
    Queue queue;
    queue.source_scene_index = plan.source_scene_index;
    queue.guid = plan.guid;
    queue.sprites.reserve(plan.object_count);
    queue.scene_action_pairs.reserve(plan.object_count);

    for (const auto& layer : plan.layers) {
        for (const auto& object : layer.objects) {
            if (object.construction_kind ==
                game_scene_construction_plan::ObjectConstructionKind::kType0SceneActionPair) {
                if (!object.type0_resource.has_value() ||
                    object.type0_resource->kind !=
                        game_scene_type0_resource::Kind::kSceneActionPair) {
                    ++queue.unresolved_object_count;
                    continue;
                }
                SceneActionPairCommand command;
                command.layer_z_index = layer.z_index;
                command.source_layer_index = layer.source_layer_index;
                command.source_object_index = object.source_object_index;
                command.resource = *object.type0_resource;
                command.layer_camera_factor = layer.first_float;
                command.record = object.record;
                queue.scene_action_pairs.push_back(std::move(command));
                ++queue.scene_action_pair_count;
                continue;
            }
            if (object.construction_kind !=
                    game_scene_construction_plan::ObjectConstructionKind::kType0SpriteBacked ||
                !object.type0_resource.has_value() ||
                !object.type0_sprite_transform.has_value()) {
                ++queue.unresolved_object_count;
                continue;
            }

            SpriteCommand command;
            command.layer_z_index = layer.z_index;
            command.source_layer_index = layer.source_layer_index;
            command.source_object_index = object.source_object_index;
            command.resource = *object.type0_resource;
            command.post_init = object.type0_post_init;
            command.transform = *object.type0_sprite_transform;
            command.layer_camera_factor = layer.first_float;

            if (!command.resource.initially_visible) {
                continue;
            }

            if (command.resource.kind == game_scene_type0_resource::Kind::kDirectFile) {
                command.direct_asset_relative_path =
                        game_scene_direct_asset::imported_relative_path(command.resource);
                if (!command.direct_asset_relative_path.has_value()) {
                    ++queue.unresolved_object_count;
                    continue;
                }
                ++queue.direct_file_count;
            } else if (command.resource.kind ==
                       game_scene_type0_resource::Kind::kSpriteFrameByName) {
                ++queue.sprite_frame_lookup_count;
            } else {
                ++queue.unresolved_object_count;
                continue;
            }
            queue.sprites.push_back(std::move(command));
        }
    }

    return queue;
}

}  // namespace nevergone::game_scene_render_queue
