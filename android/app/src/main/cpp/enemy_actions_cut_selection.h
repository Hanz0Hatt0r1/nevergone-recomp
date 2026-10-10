#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "enemy_actions_default_cut_table.h"

namespace nevergone::enemy_actions_cut_selection {

// Project-owned observation of one ActionsCut in the EnemyObject-owned cut
// array. Native ActionsCut layout is +0x14 string, +0x18 float, +0x1c float.
struct EnemyCutObservation {
    std::string frame_name_14;
    float field_18 = 0.0f;
    float field_1c = 0.0f;
};

enum class Source {
    kNone = 0,
    kEnemyObject = 1,
    kSystemDefault = 2,
};

struct Result {
    bool cut_processing_gate_open = false;
    bool used_enemy_object_array = false;
    bool used_system_default_array = false;
    bool matched = false;
    Source source = Source::kNone;
    std::size_t matched_index = 0;
    std::string frame_name_14;
    float field_18 = 0.0f;
    float field_1c = 0.0f;
    bool existing_enemy_cut_path = false;
    bool unmatched_enemy_cut_path = false;
    bool default_rect_patch_path = false;
    bool default_path_no_match = false;
};

// This is the bounded source-selection slice at 0x2ac494..0x2ac532 and the
// existing-cut branch at 0x2ad284. It runs only after the earlier retained
// object pointer gate was proven non-null.
inline Result select(
        bool cut_processing_gate_open,
        float field_278,
        const std::string& current_frame_name_68,
        const std::vector<EnemyCutObservation>& enemy_cuts,
        const std::vector<enemy_actions_default_cut_table::Entry>& default_cuts) {
    Result result;
    result.cut_processing_gate_open = cut_processing_gate_open;
    if (!cut_processing_gate_open) return result;

    // Native uses VCMPE + BGT. NaN is unordered and therefore does not take
    // the greater-than branch; C++ `field_278 > 0.0f` has the same outcome.
    if (field_278 > 0.0f) {
        result.used_enemy_object_array = true;
        for (std::size_t i = 0; i < enemy_cuts.size(); ++i) {
            if (enemy_cuts[i].frame_name_14 != current_frame_name_68) continue;

            result.matched = true;
            result.source = Source::kEnemyObject;
            result.matched_index = i;
            result.frame_name_14 = enemy_cuts[i].frame_name_14;
            result.field_18 = enemy_cuts[i].field_18;
            result.field_1c = enemy_cuts[i].field_1c;
            result.existing_enemy_cut_path = true;
            return result;
        }

        // Native does not fall back to system+0x27c in this mode. It continues
        // to the unmatched path where sprite-frame presence and later code
        // decide whether a new EnemyObject-owned ActionsCut is created.
        result.unmatched_enemy_cut_path = true;
        return result;
    }

    result.used_system_default_array = true;
    const auto match = enemy_actions_default_cut_table::first_match(
            default_cuts, current_frame_name_68);
    if (!match.found) {
        result.default_path_no_match = true;
        return result;
    }

    const auto& cut = default_cuts[match.index];
    result.matched = true;
    result.source = Source::kSystemDefault;
    result.matched_index = match.index;
    result.frame_name_14 = cut.frame_name_14;
    result.field_1c = cut.rect_height_1c;
    result.default_rect_patch_path = true;
    return result;
}

}  // namespace nevergone::enemy_actions_cut_selection
