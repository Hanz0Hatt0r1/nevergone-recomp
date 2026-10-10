#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "game_scene_construction_plan.h"
#include "game_scene_direct_asset.h"

namespace nevergone::game_scene_render_queue {

struct SpriteCommand {
    std::size_t layer_z_index = 0;
    std::size_t source_layer_index = 0;
    std::size_t source_object_index = 0;
    game_scene_type0_resource::Selection resource;
    // Present only for evidence-backed CCSprite::create(basename) resources.
    // The path is relative to filesDir/assets, matching OriginalObbImporter.
    std::optional<std::string> direct_asset_relative_path;
    game_scene_construction_plan::Type0SpriteTransform transform;
};

struct Queue {
    std::size_t source_scene_index = 0;
    std::string guid;
    std::vector<SpriteCommand> sprites;
    std::size_t sprite_frame_lookup_count = 0;
    std::size_t direct_file_count = 0;
    std::size_t scene_action_pair_count = 0;
    std::size_t unresolved_object_count = 0;
};

inline Queue build(const game_scene_construction_plan::ScenePlan& plan) {
    Queue queue;
    queue.source_scene_index = plan.source_scene_index;
    queue.guid = plan.guid;
    queue.sprites.reserve(plan.object_count);

    for (const auto& layer : plan.layers) {
        for (const auto& object : layer.objects) {
            if (object.construction_kind ==
                game_scene_construction_plan::ObjectConstructionKind::kType0SceneActionPair) {
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
            command.transform = *object.type0_sprite_transform;

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
