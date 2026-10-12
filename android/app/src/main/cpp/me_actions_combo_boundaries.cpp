#include "me_actions_combo_boundaries.h"

#include <cstdint>
#include <cstring>

namespace nevergone::me_actions_combo_boundaries {
namespace {

std::int32_t add_one_wrapping(std::int32_t value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    ++bits;
    std::int32_t result = 0;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

enemy_actions_wbg_combo_section::DerivedActionComboValue make_boundary(
        std::size_t target_offset,
        std::size_t tuple_index,
        std::int32_t previous_boundary,
        std::int32_t previous_state) {
    enemy_actions_wbg_combo_section::DerivedActionComboValue value;
    value.target_array_offset = target_offset;
    value.source_tuple_index = tuple_index;
    value.field_14 = add_one_wrapping(previous_boundary);
    value.field_18 = static_cast<std::int32_t>(tuple_index);
    value.field_1c = previous_state;
    return value;
}

bool same_boundary_fields(
        const enemy_actions_wbg_combo_section::DerivedActionComboValue& left,
        const enemy_actions_wbg_combo_section::DerivedActionComboValue& right) {
    return left.source_tuple_index == right.source_tuple_index &&
           left.field_14 == right.field_14 &&
           left.field_18 == right.field_18 &&
           left.field_1c == right.field_1c &&
           left.field_20 == right.field_20;
}

}  // namespace

Boundaries derive(const enemy_actions_wbg_combo_section::Block& block) {
    Boundaries result;
    const std::size_t count = block.tuples.size();
    result.array_b0_values.reserve(count);
    result.array_b4_values.reserve(count);

    std::int32_t b0_state = 1;
    std::int32_t b4_state = 1;
    std::int32_t b0_previous_boundary = -1;
    std::int32_t b4_previous_boundary = -1;

    for (std::size_t tuple_index = 0; tuple_index < count; ++tuple_index) {
        const auto& tuple = block.tuples[tuple_index];
        const std::int32_t second_plus_one = add_one_wrapping(tuple.i32_values[1]);
        const std::int32_t fourth_plus_one = add_one_wrapping(tuple.i32_values[3]);
        const bool last_tuple = tuple_index + 1u == count;

        if (b0_state != fourth_plus_one || last_tuple) {
            result.array_b0_values.push_back(make_boundary(
                kArrayB0Offset,
                tuple_index,
                b0_previous_boundary,
                b0_state));
            b0_previous_boundary = static_cast<std::int32_t>(tuple_index);
            b0_state = fourth_plus_one;
        }

        if (b4_state != second_plus_one || last_tuple) {
            result.array_b4_values.push_back(make_boundary(
                kArrayB4Offset,
                tuple_index,
                b4_previous_boundary,
                b4_state));
            b4_previous_boundary = static_cast<std::int32_t>(tuple_index);
            b4_state = second_plus_one;
        }
    }

    return result;
}

bool structurally_matches_existing_section_f_derivations(
        const enemy_actions_wbg_combo_section::Block& block,
        const Boundaries& boundaries) {
    // EnemyActionsData's +0x94 derivation and MeActionsData's +0xb0 derivation
    // use the same fourth-int run algorithm. Likewise enemy +0x98 and player
    // +0xb4 both use the second-int run algorithm. Only the destination array
    // offsets differ between the two runtime object layouts.
    if (boundaries.array_b0_values.size() != block.array_94_values.size() ||
        boundaries.array_b4_values.size() != block.array_98_values.size()) {
        return false;
    }
    for (std::size_t index = 0; index < boundaries.array_b0_values.size(); ++index) {
        if (!same_boundary_fields(boundaries.array_b0_values[index],
                                  block.array_94_values[index])) {
            return false;
        }
    }
    for (std::size_t index = 0; index < boundaries.array_b4_values.size(); ++index) {
        if (!same_boundary_fields(boundaries.array_b4_values[index],
                                  block.array_98_values[index])) {
            return false;
        }
    }
    return true;
}

}  // namespace nevergone::me_actions_combo_boundaries
