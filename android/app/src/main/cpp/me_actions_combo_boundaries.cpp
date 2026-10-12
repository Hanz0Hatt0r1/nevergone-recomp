#include "me_actions_combo_boundaries.h"

#include <utility>

namespace nevergone::me_actions_combo_boundaries {
namespace {

Boundary copy_boundary(
        std::size_t target_offset,
        const enemy_actions_wbg_combo_section::DerivedActionComboValue& source) {
    Boundary value;
    value.target_array_offset = target_offset;
    value.source_tuple_index = source.source_tuple_index;
    value.field_14 = source.field_14;
    value.field_18 = source.field_18;
    value.field_1c = source.field_1c;
    return value;
}

}  // namespace

bool project(const enemy_actions_wbg_document::Document& document, Projection* out) {
    if (out == nullptr) return false;
    const auto& block = document.combo_block;
    if (block.primary_record_count != document.prefix.action_frame_count ||
        block.tuples.size() != static_cast<std::size_t>(block.primary_record_count)) {
        return false;
    }

    Projection projected;
    projected.primary_b0.reserve(block.array_94_values.size());
    projected.secondary_b4.reserve(block.array_98_values.size());

    for (const auto& source : block.array_94_values) {
        if (source.source_tuple_index >= block.tuples.size()) return false;
        projected.primary_b0.push_back(copy_boundary(kPrimaryBoundaryArrayOffset, source));
    }
    for (const auto& source : block.array_98_values) {
        if (source.source_tuple_index >= block.tuples.size()) return false;
        projected.secondary_b4.push_back(copy_boundary(kSecondaryBoundaryArrayOffset, source));
    }

    *out = std::move(projected);
    return true;
}

}  // namespace nevergone::me_actions_combo_boundaries
