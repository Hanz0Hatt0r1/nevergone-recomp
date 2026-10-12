#include "trap_actions_document.h"

#include <utility>

namespace nevergone::trap_actions_document {
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
    if (!trap_actions_core_document::parse(reader, &parsed.core)) return false;

    std::size_t offset = parsed.core.bytes_consumed;
    const std::uint32_t primary_count = parsed.core.primary.prefix.record_count;

    if (!trap_actions_combo_section::parse(
                reader, offset, primary_count, &parsed.combo) ||
        !advance_offset(reader.size(), parsed.combo.bytes_consumed, &offset)) {
        return false;
    }

    if (!enemy_actions_wbg_final_table::parse(
                reader, offset, primary_count, &parsed.final_table) ||
        !advance_offset(reader.size(), parsed.final_table.bytes_consumed, &offset)) {
        return false;
    }

    parsed.bytes_consumed = offset;
    parsed.trailing_bytes = reader.size() - offset;
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::trap_actions_document
