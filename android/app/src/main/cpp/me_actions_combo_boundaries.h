#pragma once

#include <cstddef>
#include <vector>

#include "enemy_actions_wbg_combo_section.h"

namespace nevergone::me_actions_combo_boundaries {

constexpr std::size_t kArrayB0Offset = 0xb0u;
constexpr std::size_t kArrayB4Offset = 0xb4u;

struct Boundaries {
    std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue> array_b0_values;
    std::vector<enemy_actions_wbg_combo_section::DerivedActionComboValue> array_b4_values;
};

// MeActionsData::loadWBGFileWithHPData consumes the same serialized Section-F
// tuples as EnemyActionsData but materializes two player ActionComboValue arrays
// at +0xb0/+0xb4. The +0xb0 run key is tuple int[3]+1; +0xb4 is int[1]+1.
// Each emitted ActionComboValue stores previous_boundary+1 at +0x14, the current
// tuple/frame index at +0x18 and the previous run state at +0x1c.
Boundaries derive(const enemy_actions_wbg_combo_section::Block& block);

bool structurally_matches_existing_section_f_derivations(
    const enemy_actions_wbg_combo_section::Block& block,
    const Boundaries& boundaries);

}  // namespace nevergone::me_actions_combo_boundaries
