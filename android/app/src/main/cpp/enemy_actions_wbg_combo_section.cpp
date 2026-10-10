#include "enemy_actions_wbg_combo_section.h"

#include <cstring>
#include <utility>

#include "enemy_actions_layout_evidence.h"

namespace nevergone::enemy_actions_wbg_combo_section {
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

DerivedActionComboValue make_boundary_value(
        std::size_t target_array_offset,
        std::size_t tuple_index,
        std::int32_t previous_boundary_index,
        std::int32_t current_state) {
    DerivedActionComboValue value;
    value.target_array_offset = target_array_offset;
    value.source_tuple_index = tuple_index;
    value.field_14 = add_one_wrapping(previous_boundary_index);
    value.field_18 = static_cast<std::int32_t>(tuple_index);
    value.field_1c = current_state;
    return value;
}

}  // namespace

bool parse(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::uint32_t primary_record_count,
        Block* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    const std::size_t count = static_cast<std::size_t>(primary_record_count);
    if (count > enemy_actions_layout_evidence::kObservedSampleCount) return false;

    hp_data::Cursor cursor(reader, start_offset);
    if (count > cursor.remaining() / kTupleBytes) return false;

    Block parsed;
    parsed.primary_record_count = primary_record_count;
    parsed.tuples.reserve(count);
    parsed.array_94_values.reserve(count);
    parsed.array_98_values.reserve(count);
    parsed.array_9c_values.reserve(count);

    // Native loop state at 0x29023c..0x290250.
    std::int32_t array_94_state = 1;
    std::int32_t array_98_state = 1;
    std::int32_t array_94_previous_boundary = -1;
    std::int32_t array_98_previous_boundary = -1;

    for (std::size_t tuple_index = 0; tuple_index < count; ++tuple_index) {
        Tuple tuple;
        for (std::int32_t& value : tuple.i32_values) {
            if (!cursor.read_i32_le(&value)) return false;
        }
        for (float& value : tuple.float_values) {
            if (!cursor.read_f32_le(&value)) return false;
        }

        if (!enemy_actions_layout_evidence::first_observed_region_element_offset(
                    tuple_index,
                    &tuple.first_float_target_offset) ||
            !enemy_actions_layout_evidence::second_observed_region_element_offset(
                    tuple_index,
                    &tuple.second_float_target_offset)) {
            return false;
        }

        // The original increments tuple integers 1, 2 and 3 immediately after
        // reading them. ARM32 ADD wraps modulo 2^32, so mirror that explicitly.
        const std::int32_t second_plus_one = add_one_wrapping(tuple.i32_values[1]);
        const std::int32_t third_plus_one = add_one_wrapping(tuple.i32_values[2]);
        const std::int32_t fourth_plus_one = add_one_wrapping(tuple.i32_values[3]);
        const bool last_tuple = tuple_index + 1u == count;

        // 0x29033c..0x290372: emit to EnemyActionsData+0x94 whenever the
        // incremented fourth integer changes, and always close the final run.
        if (array_94_state != fourth_plus_one || last_tuple) {
            parsed.array_94_values.push_back(make_boundary_value(
                    kArray94Offset,
                    tuple_index,
                    array_94_previous_boundary,
                    array_94_state));
            array_94_previous_boundary = static_cast<std::int32_t>(tuple_index);
            array_94_state = fourth_plus_one;
        }

        // 0x290374..0x290384: one object is always appended to +0x9c. Only
        // field +0x1c is overwritten after initACV(), with integer 2 plus one.
        DerivedActionComboValue every_tuple;
        every_tuple.target_array_offset = kArray9cOffset;
        every_tuple.source_tuple_index = tuple_index;
        every_tuple.field_1c = third_plus_one;
        parsed.array_9c_values.push_back(every_tuple);

        // 0x290388..0x2903bc: equivalent run-boundary logic for the
        // incremented second integer, targeting EnemyActionsData+0x98.
        if (array_98_state != second_plus_one || last_tuple) {
            parsed.array_98_values.push_back(make_boundary_value(
                    kArray98Offset,
                    tuple_index,
                    array_98_previous_boundary,
                    array_98_state));
            array_98_previous_boundary = static_cast<std::int32_t>(tuple_index);
            array_98_state = second_plus_one;
        }

        parsed.tuples.push_back(tuple);
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    if (parsed.bytes_consumed != count * kTupleBytes) return false;

    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::enemy_actions_wbg_combo_section
