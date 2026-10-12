#pragma once

#include <cstddef>

#include "enemy_actions_wbg_final_table.h"
#include "hp_data_reader.h"
#include "trap_actions_combo_section.h"
#include "trap_actions_core_document.h"

namespace nevergone::trap_actions_document {

struct Document {
    trap_actions_core_document::Document core;
    trap_actions_combo_section::Block combo;
    enemy_actions_wbg_final_table::Table final_table;
    std::size_t bytes_consumed = 0;
    std::size_t trailing_bytes = 0;
};

// Compose all currently recovered TrapActionsData::loadWBGFile() serialized
// sections. The final per-primary reciprocal table is wire-compatible with the
// already recovered EnemyActions table. Output is transactional and trailing
// bytes are reported instead of rejected because the original EOF policy is
// not required by the recovered loader path.
bool parse(const hp_data::Reader& reader, Document* out);

}  // namespace nevergone::trap_actions_document
