#pragma once

#include <cstddef>

#include "enemy_actions_wbg_prefix.h"
#include "hp_data_reader.h"
#include "trap_actions_compact.h"
#include "trap_actions_primary.h"

namespace nevergone::trap_actions_core_document {

struct Document {
    trap_actions_primary::Block primary;
    trap_actions_compact::Block compact;
    enemy_actions_wbg_prefix::NestedActionFrameBlock nested;
    enemy_actions_wbg_prefix::VersionedActionFrameBlock versioned;
    enemy_actions_wbg_prefix::FixedTailActionFrameBlock fixed_tail;
    std::size_t bytes_consumed = 0;
    std::size_t trailing_bytes = 0;
};

// Compose the evidence-backed TrapActions sections from file start through the
// fixed-tail ActionFrameData block. The nested/versioned/fixed-tail wire formats
// are byte-compatible with the already recovered EnemyActions sections; only
// the preceding compact block needs Trap-specific unsigned count semantics.
// Output is transactional and EOF is not required after this recovered boundary.
bool parse(const hp_data::Reader& reader, Document* out);

}  // namespace nevergone::trap_actions_core_document
