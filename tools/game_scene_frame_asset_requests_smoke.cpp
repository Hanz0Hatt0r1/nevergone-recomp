#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#include "game_scene_frame_asset_requests.h"

namespace {

nevergone::game_scene_render_queue::SpriteCommand frame_command(const char* name) {
    nevergone::game_scene_render_queue::SpriteCommand command;
    command.resource.kind = nevergone::game_scene_type0_resource::Kind::kSpriteFrameByName;
    command.resource.resource_name = name;
    return command;
}

nevergone::game_scene_render_queue::SpriteCommand direct_command(const char* name) {
    nevergone::game_scene_render_queue::SpriteCommand command;
    command.resource.kind = nevergone::game_scene_type0_resource::Kind::kDirectFile;
    command.resource.resource_name = name;
    command.direct_asset_relative_path = std::string("gamescene/gs_res_image_file/") + name;
    return command;
}

}  // namespace

int main() {
    namespace requests = nevergone::game_scene_frame_asset_requests;

    nevergone::game_scene_render_queue::Queue queue;
    queue.source_scene_index = 4u;
    queue.guid = "scene-a";
    queue.sprites.push_back(frame_command("tree01.png"));
    queue.sprites.push_back(direct_command("gktianchong.png"));
    queue.sprites.push_back(frame_command("fog02.png"));
    queue.sprite_frame_lookup_count = 2u;
    queue.direct_file_count = 1u;

    const auto first = requests::build(queue);
    assert(first.source_scene_index == 4u);
    assert(first.guid == "scene-a");
    assert(first.revision != 0u);
    assert(first.requests.size() == 2u);
    assert(first.requests[0].sprite_command_index == 0u);
    assert(first.requests[0].frame_name == "tree01.png");
    assert(first.requests[1].sprite_command_index == 2u);
    assert(first.requests[1].frame_name == "fog02.png");

    const auto repeat = requests::build(queue);
    assert(repeat.revision == first.revision);

    queue.sprites[2].resource.resource_name = "fog03.png";
    const auto changed_name = requests::build(queue);
    assert(changed_name.revision != first.revision);

    queue.sprites[2].resource.resource_name = "fog02.png";
    queue.guid = "scene-b";
    const auto changed_scene = requests::build(queue);
    assert(changed_scene.revision != first.revision);

    nevergone::game_scene_render_queue::Queue empty;
    empty.source_scene_index = 9u;
    empty.guid = "empty";
    empty.sprites.push_back(direct_command("gkyuanjing.png"));
    empty.direct_file_count = 1u;
    const auto no_frames = requests::build(empty);
    assert(no_frames.revision != 0u);
    assert(no_frames.requests.empty());

    return 0;
}
