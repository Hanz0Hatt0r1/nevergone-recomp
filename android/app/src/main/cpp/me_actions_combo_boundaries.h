#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "enemy_actions_wbg_document.h"

namespace nevergone::me_actions_combo_boundaries {

constexpr std::size_t kPrimaryBoundaryArrayOffset = 0xb0u;
constexpr std::size_t kSecondaryBoundaryArrayOffset = 0xb4u;

struct Boundary {
    std::size_t target_array_offset = 0;
    std::size_t source_tuple_index = 0;
    std::int32_t field_14 = 0;
    std::int32_t field_18 = 0;
    std::int32_t field_1c = 0;
};

struct Projection {
    std::vector<Boundary> primary_b0;
    std::vector<Boundary> secondary_b4;
};

// MeActionsData::loadWBGFileWithHPData consumes the same Section-F tuples as
// the existing parser. Its +0xb0 boundary run is driven by tuple int32[3]+1;
// +0xb4 is driven by tuple int32[1]+1. Those are exactly the already-proven
// array_94/array_98 run-boundary sequences, so this projection reuses the
// parsed values rather than reparsing or inventing a second serializer model.
bool project(const enemy_actions_wbg_document::Document& document, Projection* out);

}  // namespace nevergone::me_actions_combo_boundaries
