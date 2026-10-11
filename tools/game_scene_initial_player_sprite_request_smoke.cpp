#include <cassert>
#include <limits>
#include <string>

#include "game_scene_initial_player_sprite_request.h"

int main() {
    namespace request = nevergone::game_scene_initial_player_sprite_request;
    namespace resource = nevergone::game_scene_type0_resource;
    using nevergone::enemy_actions_wbg_document::Document;
    using nevergone::enemy_actions_wbg_prefix::ActionFrameRecord;

    Document document;
    ActionFrameRecord frame;
    frame.third_string = "player_idle_001.png";
    frame.third_string_length_i32 = 19;
    frame.float_values[0] = 12.0f;
    frame.float_values[1] = -4.0f;
    frame.float_values[2] = 0.5f;
    frame.float_values[3] = 0.25f;
    frame.float_values[6] = 20.0f;
    frame.float_values[7] = 10.0f;
    frame.float_values[8] = 7.0f;
    frame.float_values[9] = 15.0f;
    frame.float_values[10] = 1.0f;
    frame.float_values[11] = 1.0f;
    document.primary_records.push_back(frame);

    nevergone::game_scene_player_bootstrap::InitialPoint player_position;
    player_position.x = 600.0f;
    player_position.y = 468.0f;

    const auto normal = request::build(document, player_position);
    assert(normal.status == request::Status::kReady);
    assert(normal.frame_request.resource.has_value());
    assert(normal.frame_request.resource->kind == resource::Kind::kSpriteFrameByName);
    assert(normal.frame_request.resource->resource_name == "player_idle_001.png");
    assert(normal.frame_request.position.x == 600.0f);
    assert(normal.frame_request.position.y == 468.0f);
    assert(normal.sprite_transform.has_value());
    assert(normal.sprite_transform->position_x == 12.0f);
    assert(normal.sprite_transform->position_y == -4.0f);
    assert(normal.sprite_transform->anchor_x == 0.5f);
    assert(normal.sprite_transform->anchor_y == 0.25f);
    assert(normal.sprite_transform->rotation == 15.0f);
    assert(normal.sprite_transform->child_z_order == 7);
    assert(std::string(request::status_name(normal.status)) == "ready");

    document.primary_records[0].first_bool = true;
    const auto flipped = request::build(document, player_position);
    assert(flipped.status == request::Status::kReady);
    assert(flipped.sprite_transform.has_value());
    assert(flipped.sprite_transform->flip_x);
    assert(flipped.sprite_transform->position_x == 28.0f);  // 2*20 - 12
    assert(flipped.sprite_transform->position_y == -4.0f);
    assert(flipped.sprite_transform->anchor_x == 0.5f);
    assert(flipped.sprite_transform->rotation == 15.0f);
    assert(flipped.sprite_transform->rotation_during_move_update == -15.0f);

    Document empty;
    const auto no_frame = request::build(empty, player_position);
    assert(no_frame.status == request::Status::kFrameRequestFailed);
    assert(!no_frame.sprite_transform.has_value());

    document.primary_records[0].third_string = "../bad.png";
    document.primary_records[0].third_string_length_i32 = 10;
    const auto bad_key = request::build(document, player_position);
    assert(bad_key.status == request::Status::kFrameRequestFailed);

    document.primary_records[0].third_string = "player_idle_001.png";
    document.primary_records[0].third_string_length_i32 = 19;
    document.primary_records[0].float_values[0] =
            std::numeric_limits<float>::quiet_NaN();
    const auto bad_transform = request::build(document, player_position);
    assert(bad_transform.status == request::Status::kInvalidSpriteTransform);
    assert(!bad_transform.sprite_transform.has_value());

    return 0;
}
