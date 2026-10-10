#include "enemy_actions_wbg_final_table.h"

#include <utility>

namespace nevergone::enemy_actions_wbg_final_table {

bool parse(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::uint32_t primary_record_count,
        Table* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    const std::size_t count = static_cast<std::size_t>(primary_record_count);
    if (count > cursor.remaining() / kEntryBytes) return false;

    Table parsed;
    parsed.primary_record_count = primary_record_count;
    parsed.entries.reserve(count);

    for (std::size_t index = 0; index < count; ++index) {
        (void) index;
        Entry entry;
        if (!cursor.read_i32_le(&entry.serialized_value)) return false;
        entry.reciprocal_value = 1.0f / static_cast<float>(entry.serialized_value);
        parsed.entries.push_back(entry);
    }

    parsed.bytes_consumed = cursor.offset() - start_offset;
    if (parsed.bytes_consumed != count * kEntryBytes) return false;

    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::enemy_actions_wbg_final_table
