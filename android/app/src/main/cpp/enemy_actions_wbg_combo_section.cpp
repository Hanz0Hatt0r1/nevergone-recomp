#include "enemy_actions_wbg_combo_section.h"

#include <utility>

#include "enemy_actions_layout_evidence.h"

namespace nevergone::enemy_actions_wbg_combo_section {

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
        parsed.tuples.push_back(tuple);
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    if (parsed.bytes_consumed != count * kTupleBytes) return false;

    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::enemy_actions_wbg_combo_section
