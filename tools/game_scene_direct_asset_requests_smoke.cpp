#include <cassert>
#include <string>

#include "game_scene_direct_asset_requests.h"

namespace {
nevergone::game_scene_render_queue::SpriteCommand direct_command(
        const std::string& path,
        std::size_t object_index) {
    nevergone::game_scene_render_queue::SpriteCommand command;
    command.source_object_index = object_index;
    command.resource = {
        nevergone::game_scene_type0_resource::Kind::kDirectFile,
        "direct.png"};
    command.direct_asset_relative_path = path;
    return command;
}

nevergone::game_scene_render_queue::SpriteCommand frame_command(
        const std::string& name,
        std::size_t object_index,
        bool player = false) {
    nevergone::game_scene_render_queue::SpriteCommand command;
    command.source_object_index = object_index;
    command.resource = {
        nevergone::game_scene_type0_resource::Kind::kSpriteFrameByName,
        name};
    if (player) {
        command.player_body_transform =
                nevergone::me_actions_initial_sprite_transform::Plan{};
    }
    return command;
}
}  // namespace

int main() {
    namespace requests = nevergone::game_scene_direct_asset_requests;
    namespace queue_ns = nevergone::game_scene_render_queue;

    queue_ns::Queue queue;
    queue.source_scene_index = 7u;
    queue.guid = "scene-a";
    queue.direct_file_count = 2u;
    queue.sprite_frame_lookup_count = 2u;

    queue.sprites.push_back(frame_command("atlas-frame.png", 0u));
    queue.sprites.push_back(frame_command("player-frame.png", 99u, true));
    queue.sprites.push_back(direct_command(
        "gamescene/gs_res_image_file/gktianchong.png", 1u));
    queue.sprites.push_back(direct_command(
        "gamescene/gs_res_image_file/gkyuanjing.png", 2u));

    const auto first = requests::build(queue);
    const auto second = requests::build(queue);
    assert(first.source_scene_index == 7u);
    assert(first.guid == "scene-a");
    assert(first.revision != 0u);
    assert(first.revision == second.revision);
    assert(first.requests.size() == 4u);
    assert(first.requests[0].kind == requests::Kind::kSpriteFrameByName);
    assert(first.requests[0].sprite_command_index == 0u);
    assert(first.requests[0].frame_name == "atlas-frame.png");
    assert(first.requests[0].relative_path.empty());
    assert(first.requests[1].kind == requests::Kind::kPlayerSpriteFrameByName);
    assert(first.requests[1].sprite_command_index == 1u);
    assert(first.requests[1].frame_name == "player-frame.png");
    assert(first.requests[1].relative_path.empty());
    assert(first.requests[2].kind == requests::Kind::kDirectFile);
    assert(first.requests[2].sprite_command_index == 2u);
    assert(first.requests[2].relative_path ==
           "gamescene/gs_res_image_file/gktianchong.png");
    assert(first.requests[2].frame_name.empty());
    assert(first.requests[3].kind == requests::Kind::kDirectFile);
    assert(first.requests[3].sprite_command_index == 3u);
    assert(first.requests[3].relative_path ==
           "gamescene/gs_res_image_file/gkyuanjing.png");

    // Request domain is part of the revision. The same frame name moving from
    // a scene atlas to the player atlas set must invalidate the staged snapshot.
    auto changed_domain = queue;
    changed_domain.sprites[0].player_body_transform =
            nevergone::me_actions_initial_sprite_transform::Plan{};
    assert(requests::build(changed_domain).requests[0].kind ==
           requests::Kind::kPlayerSpriteFrameByName);
    assert(requests::build(changed_domain).revision != first.revision);

    auto changed_path = queue;
    changed_path.sprites[3].direct_asset_relative_path =
        "gamescene/gs_res_image_file/czyanwu1.png";
    assert(requests::build(changed_path).revision != first.revision);

    auto changed_frame = queue;
    changed_frame.sprites[0].resource.resource_name = "atlas-frame-2.png";
    assert(requests::build(changed_frame).revision != first.revision);

    auto changed_scene = queue;
    changed_scene.guid = "scene-b";
    assert(requests::build(changed_scene).revision != first.revision);

    auto changed_index = queue;
    changed_index.sprites.insert(changed_index.sprites.begin(), frame_command("prefix.png", 9u));
    ++changed_index.sprite_frame_lookup_count;
    assert(requests::build(changed_index).revision != first.revision);

    queue_ns::Queue empty;
    empty.source_scene_index = 9u;
    empty.guid = "no-static-assets";
    const auto empty_snapshot = requests::build(empty);
    assert(empty_snapshot.revision != 0u);
    assert(empty_snapshot.requests.empty());
    return 0;
}
