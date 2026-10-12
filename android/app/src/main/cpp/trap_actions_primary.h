#pragma once

#include <cstddef>
#include <vector>

#include "enemy_actions_wbg_prefix.h"
#include "hp_data_reader.h"
#include "trap_actions_prefix.h"

namespace nevergone::trap_actions_primary {

using ActionFrameRecord = enemy_actions_wbg_prefix::ActionFrameRecord;

struct Block {
    trap_actions_prefix::Prefix prefix;
    std::vector<ActionFrameRecord> records;
    std::size_t bytes_consumed = 0;
};

// Parse the fixed TrapActions prefix followed by exactly prefix.record_count
// primary ActionFrameData records. Static inspection of TrapActionsData and
// EnemyActionsData proves this primary record wire format is identical, so the
// established bounded ActionFrameRecord parser is intentionally reused.
// Transactional: *out is unchanged unless the whole declared primary block is
// consumed successfully.
bool parse(const hp_data::Reader& reader, Block* out);

}  // namespace nevergone::trap_actions_primary
