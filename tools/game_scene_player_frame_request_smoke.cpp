#include <cassert>
#include <cmath>
#include <limits>

#include "game_scene_player_frame_request.h"

int main() {
    namespace bridge = nevergone::game_scene_player_frame_request;
    namespace document = nevergone::enemy_actions_wbg_document;
    namespace prefix = nevergone::enemy_actions_wbg_prefix;
    namespace resource = nevergone::game_scene_type0_resource;

    document::Document doc;
    prefix::ActionFrameRecord frame;
    frame.third_string = "player_start_001.png";
    frame.third_string_length_i32 = 20;
    doc.primary_records.push_back(frame);

    bridge::State state;
    state.action_document_ready = true;
    state.current_frame_1a4 = 0;

    nevergone::game_scene_player_bootstrap::InitialPoint position;
    position.x = 568.0f;
    position.y = 448.0f;

    const auto ready = bridge::build(doc, state, position);
    assert(ready.status == bridge::Status::kReady);
    assert(ready.resource.has_value());
    assert(ready.resource->kind == resource::Kind::kSpriteFrameByName);
    assert(ready.resource->resource_name == "player_start_001.png");
    assert(ready.selected_primary_index == 0u);
    assert(ready.position.x == 568.0f);
    assert(ready.position.y == 448.0f);
    assert(std::string(bridge::status_name(ready.status)) == "ready");

    state.action_document_ready = false;
    const auto closed = bridge::build(doc, state, position);
    assert(closed.status == bridge::Status::kReadinessGateClosed);
    assert(!closed.resource.has_value());

    state.action_document_ready = true;
    document::Document empty_doc;
    const auto empty = bridge::build(empty_doc, state, position);
    assert(empty.status == bridge::Status::kNoPrimaryFrames);

    state.current_frame_1a4 = 1;
    const auto out_of_range = bridge::build(doc, state, position);
    assert(out_of_range.status == bridge::Status::kFrameOutOfRange);
    state.current_frame_1a4 = 0;

    auto invalid_position = position;
    invalid_position.x = std::numeric_limits<float>::quiet_NaN();
    const auto bad_position = bridge::build(doc, state, invalid_position);
    assert(bad_position.status == bridge::Status::kInvalidPosition);

    doc.primary_records[0].third_string = "../player.png";
    doc.primary_records[0].third_string_length_i32 = 13;
    const auto traversal = bridge::build(doc, state, position);
    assert(traversal.status == bridge::Status::kInvalidFrameKey);
    assert(!traversal.resource.has_value());

    doc.primary_records[0].third_string = "player.png";
    doc.primary_records[0].third_string_length_i32 = 999;
    const auto length_mismatch = bridge::build(doc, state, position);
    assert(length_mismatch.status == bridge::Status::kInvalidFrameKey);

    doc.primary_records[0].third_string.clear();
    doc.primary_records[0].third_string_length_i32 = 0;
    const auto blank = bridge::build(doc, state, position);
    assert(blank.status == bridge::Status::kInvalidFrameKey);

    return 0;
}
