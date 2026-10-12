#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "enemy_actions_wbg_prefix.h"
#include "hp_data_reader.h"

namespace nevergone::trap_actions_compact {

using Record = enemy_actions_wbg_prefix::CompactActionFrameRecord;

struct Block {
    std::uint32_t record_count = 0;
    std::vector<Record> records;
    std::size_t bytes_consumed = 0;
};

// Parse the compact TrapActions section that immediately follows the primary
// frame array. Native compares its uint32 count with BHS, so this header is
// deliberately unsigned even though the compatible EnemyActions compact block
// uses a signed count contract.
bool parse(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        Block* out);

}  // namespace nevergone::trap_actions_compact
