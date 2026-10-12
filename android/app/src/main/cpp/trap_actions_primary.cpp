#include "trap_actions_primary.h"

#include <utility>

namespace nevergone::trap_actions_primary {

bool parse(const hp_data::Reader& reader, Block* out) {
    if (out == nullptr) return false;

    Block parsed;
    if (!trap_actions_prefix::parse(reader, &parsed.prefix)) return false;

    std::size_t cursor = parsed.prefix.bytes_consumed;
    if (cursor > reader.size()) return false;

    const std::size_t remaining = reader.size() - cursor;
    if (parsed.prefix.record_count >
            remaining / enemy_actions_wbg_prefix::kActionFrameFixedBytes) {
        return false;
    }
    parsed.records.reserve(static_cast<std::size_t>(parsed.prefix.record_count));

    for (std::uint32_t index = 0; index < parsed.prefix.record_count; ++index) {
        ActionFrameRecord record;
        if (!enemy_actions_wbg_prefix::parse_action_frame_record(
                    reader, cursor, &record) ||
            record.bytes_consumed < enemy_actions_wbg_prefix::kActionFrameFixedBytes ||
            record.bytes_consumed > reader.size() - cursor) {
            return false;
        }
        cursor += record.bytes_consumed;
        parsed.records.push_back(std::move(record));
    }

    parsed.bytes_consumed = cursor;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::trap_actions_primary
