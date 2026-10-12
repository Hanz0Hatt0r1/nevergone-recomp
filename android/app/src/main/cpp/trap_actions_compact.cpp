#include "trap_actions_compact.h"

#include <utility>

namespace nevergone::trap_actions_compact {

bool parse(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        Block* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    Block parsed;
    if (!cursor.read_u32_le(&parsed.record_count)) return false;

    const std::size_t count = static_cast<std::size_t>(parsed.record_count);
    if (count > cursor.remaining() / enemy_actions_wbg_prefix::kCompactActionFrameBytes) {
        return false;
    }
    parsed.records.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        Record record;
        if (!cursor.read_i32_le(&record.first_i32) ||
            !cursor.read_f32_le(&record.first_float) ||
            !cursor.read_f32_le(&record.second_float)) {
            return false;
        }
        parsed.records.push_back(record);
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    const std::size_t expected = enemy_actions_wbg_prefix::kCompactBlockHeaderBytes +
            count * enemy_actions_wbg_prefix::kCompactActionFrameBytes;
    if (parsed.bytes_consumed != expected) return false;

    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::trap_actions_compact
