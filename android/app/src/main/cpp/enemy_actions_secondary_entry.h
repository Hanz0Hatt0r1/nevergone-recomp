#pragma once

#include <cstddef>
#include <cstdint>

#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_secondary_entry {

struct Result {
    bool enemy_actions_data_present = false;
    bool secondary_array_present = false;
    std::size_t secondary_count = 0;
    bool native_would_object_at_index = false;
    std::uint32_t requested_index = 0;
    bool selected_record_available = false;
    float selected_field_18 = 0.0f;
};

// Reconstructs the entry/selection slice at 0x2ac83c..0x2ac86e.
// Native calls objectAtIndex(current_frame_294) whenever EAD+0x8c exists and
// count() is nonzero; there is no additional current-frame range check here.
inline Result apply(
        std::int32_t current_frame_294,
        bool enemy_actions_data_present,
        bool secondary_array_present,
        const enemy_actions_wbg_prefix::CompactActionFrameBlock& block) {
    Result out;
    out.enemy_actions_data_present = enemy_actions_data_present;
    out.secondary_array_present = secondary_array_present;
    out.secondary_count = block.records.size();
    out.requested_index = static_cast<std::uint32_t>(current_frame_294);

    if (!enemy_actions_data_present || !secondary_array_present) return out;
    if (block.records.empty()) return out;

    out.native_would_object_at_index = true;

    // Project-owned parsed storage must not emulate an out-of-range CCArray
    // access. Report native intent separately from safely available data.
    const std::size_t index = static_cast<std::size_t>(out.requested_index);
    if (index >= block.records.size()) return out;

    out.selected_record_available = true;
    // Section-B construction maps the second serialized float to AFD+0x18.
    out.selected_field_18 = block.records[index].second_float;
    return out;
}

}  // namespace nevergone::enemy_actions_secondary_entry
