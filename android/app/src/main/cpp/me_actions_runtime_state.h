#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "enemy_actions_wbg_document.h"

namespace nevergone::me_actions_runtime_state {

// MeActionsSystem keeps its player-animation frame cursor at +0x1a4 and the
// previous frame at +0x1a8. initWithFile() and play() initialize these to 0 and
// -1 respectively before play() calls updateData(). Keep this state separate
// from EnemyActionsSystem, whose recovered layout uses different offsets and
// readiness gates.
struct State {
    std::int32_t current_frame_1a4 = 0;
    std::int32_t previous_frame_1a8 = -1;
};

struct UpdateDataFrameLookupResult {
    State state;
    bool has_primary_frames = false;
    bool current_frame_in_range = false;
    std::size_t selected_primary_index = 0;
    std::string frame_name_68;
    std::int32_t frame_name_length_6c = 0;
    bool should_lookup_sprite_frame = false;
};

inline State initial_play_state() {
    return State{};
}

// Models only the bounded frame-selection prefix shared by the recovered
// MeActionsSystem::play()/updateData() path. The parsed action document exposes
// the same ActionFrameData +0x68 string as primary_records[].third_string.
inline UpdateDataFrameLookupResult apply_update_data_frame_lookup(
        const enemy_actions_wbg_document::Document& document,
        State state) {
    UpdateDataFrameLookupResult result;
    result.state = state;
    result.has_primary_frames = !document.primary_records.empty();
    if (!result.has_primary_frames || state.current_frame_1a4 < 0) return result;

    const std::size_t frame_index = static_cast<std::size_t>(state.current_frame_1a4);
    if (frame_index >= document.primary_records.size()) return result;

    result.current_frame_in_range = true;
    result.selected_primary_index = frame_index;
    const auto& frame = document.primary_records[frame_index];
    result.frame_name_68 = frame.third_string;
    result.frame_name_length_6c = frame.third_string_length_i32;
    result.should_lookup_sprite_frame = true;
    return result;
}

}  // namespace nevergone::me_actions_runtime_state
