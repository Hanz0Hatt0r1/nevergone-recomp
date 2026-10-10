#include "enemy_actions_wbg_document.h"

#include <utility>

namespace nevergone::enemy_actions_wbg_document {
namespace {

bool advance_offset(
        std::size_t reader_size,
        std::size_t consumed,
        std::size_t* offset) {
    if (offset == nullptr || *offset > reader_size) return false;
    if (consumed > reader_size - *offset) return false;
    *offset += consumed;
    return true;
}

}  // namespace

bool parse(const hp_data::Reader& reader, Document* out) {
    if (out == nullptr) return false;

    Document parsed;
    if (!enemy_actions_wbg_prefix::parse_prefix(reader, &parsed.prefix)) return false;

    std::size_t offset = parsed.prefix.bytes_consumed;
    const std::size_t primary_count =
            static_cast<std::size_t>(parsed.prefix.action_frame_count);
    if (primary_count > (reader.size() - offset) /
                                enemy_actions_wbg_prefix::kActionFrameFixedBytes) {
        return false;
    }
    parsed.primary_records.reserve(primary_count);

    for (std::size_t i = 0; i < primary_count; ++i) {
        (void) i;
        enemy_actions_wbg_prefix::ActionFrameRecord record;
        if (!enemy_actions_wbg_prefix::parse_action_frame_record(reader, offset, &record)) {
            return false;
        }
        if (!advance_offset(reader.size(), record.bytes_consumed, &offset)) return false;
        parsed.primary_records.push_back(std::move(record));
    }

    if (!enemy_actions_wbg_prefix::parse_compact_action_frame_block(
                reader, offset, &parsed.compact_block) ||
        !advance_offset(reader.size(), parsed.compact_block.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_prefix::parse_nested_action_frame_block(
                reader, offset, &parsed.nested_block) ||
        !advance_offset(reader.size(), parsed.nested_block.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_prefix::parse_versioned_action_frame_groups(
                reader,
                offset,
                parsed.prefix.first_i32,
                &parsed.versioned_block) ||
        !advance_offset(reader.size(), parsed.versioned_block.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_prefix::parse_fixed_tail_action_frame_block(
                reader, offset, &parsed.fixed_tail_block) ||
        !advance_offset(reader.size(), parsed.fixed_tail_block.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_combo_section::parse(
                reader,
                offset,
                parsed.prefix.action_frame_count,
                &parsed.combo_block) ||
        !advance_offset(reader.size(), parsed.combo_block.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_final_table::parse(
                reader,
                offset,
                parsed.prefix.action_frame_count,
                &parsed.final_table) ||
        !advance_offset(reader.size(), parsed.final_table.bytes_consumed, &offset)) {
        return false;
    }

    parsed.bytes_consumed = offset;
    parsed.trailing_bytes = reader.size() - offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::enemy_actions_wbg_document
