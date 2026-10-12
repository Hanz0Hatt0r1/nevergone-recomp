#include "trap_actions_core_document.h"

#include <utility>

namespace nevergone::trap_actions_core_document {
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
    if (!trap_actions_primary::parse(reader, &parsed.primary)) return false;

    std::size_t offset = parsed.primary.bytes_consumed;
    if (!trap_actions_compact::parse(reader, offset, &parsed.compact) ||
        !advance_offset(reader.size(), parsed.compact.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_prefix::parse_nested_action_frame_block(
                reader, offset, &parsed.nested) ||
        !advance_offset(reader.size(), parsed.nested.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_prefix::parse_versioned_action_frame_groups(
                reader,
                offset,
                parsed.primary.prefix.first_i32,
                &parsed.versioned) ||
        !advance_offset(reader.size(), parsed.versioned.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_prefix::parse_fixed_tail_action_frame_block(
                reader, offset, &parsed.fixed_tail) ||
        !advance_offset(reader.size(), parsed.fixed_tail.bytes_consumed, &offset)) {
        return false;
    }

    parsed.bytes_consumed = offset;
    parsed.trailing_bytes = reader.size() - offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::trap_actions_core_document
