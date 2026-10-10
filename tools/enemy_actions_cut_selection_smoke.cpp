#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

#include "enemy_actions_cut_selection.h"

int main() {
    namespace cut = nevergone::enemy_actions_cut_selection;
    namespace defaults = nevergone::enemy_actions_default_cut_table;

    std::vector<cut::EnemyCutObservation> enemy_cuts{
            {"other.png", 1.0f, 10.0f},
            {"frame.png", 2.5f, 20.0f},
            {"frame.png", 3.5f, 30.0f},
    };

    std::vector<defaults::Entry> default_cuts{
            {0u, "frame.png", 9, 44.0f},
            {1u, "frame.png", 9, 55.0f},
            {2u, "third.png", 9, 66.0f},
    };

    // Positive +0x278 selects the EnemyObject-owned ActionsCut array and keeps
    // first-match behavior for duplicate keys.
    auto result = cut::select(true, 1.0f, "frame.png", enemy_cuts, default_cuts);
    assert(result.cut_processing_gate_open);
    assert(result.used_enemy_object_array);
    assert(!result.used_system_default_array);
    assert(result.matched);
    assert(result.source == cut::Source::kEnemyObject);
    assert(result.matched_index == 1u);
    assert(result.field_18 == 2.5f);
    assert(result.field_1c == 20.0f);
    assert(result.existing_enemy_cut_path);
    assert(!result.unmatched_enemy_cut_path);
    assert(!result.default_rect_patch_path);

    // A miss on the positive branch must not fall back to system+0x27c.
    result = cut::select(true, 1.0f, "third.png", enemy_cuts, default_cuts);
    assert(result.used_enemy_object_array);
    assert(!result.used_system_default_array);
    assert(!result.matched);
    assert(result.unmatched_enemy_cut_path);
    assert(!result.default_rect_patch_path);

    // Zero and negative values select the system default table.
    result = cut::select(true, 0.0f, "frame.png", enemy_cuts, default_cuts);
    assert(!result.used_enemy_object_array);
    assert(result.used_system_default_array);
    assert(result.matched);
    assert(result.source == cut::Source::kSystemDefault);
    assert(result.matched_index == 0u);
    assert(result.field_1c == 44.0f);
    assert(result.default_rect_patch_path);

    result = cut::select(true, -1.0f, "missing.png", enemy_cuts, default_cuts);
    assert(result.used_system_default_array);
    assert(!result.matched);
    assert(result.default_path_no_match);

    // Native VCMPE/BGT does not take the greater-than branch for NaN.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    result = cut::select(true, nan, "third.png", enemy_cuts, default_cuts);
    assert(result.used_system_default_array);
    assert(result.matched);
    assert(result.source == cut::Source::kSystemDefault);
    assert(result.matched_index == 2u);

    // A null retained object from the earlier +0x250/vtable+0xcc gate skips
    // this entire source-selection slice regardless of +0x278.
    result = cut::select(false, 5.0f, "frame.png", enemy_cuts, default_cuts);
    assert(!result.cut_processing_gate_open);
    assert(!result.used_enemy_object_array);
    assert(!result.used_system_default_array);
    assert(!result.matched);

    return 0;
}
