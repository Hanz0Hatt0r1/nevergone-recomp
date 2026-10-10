#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "enemy_actions_wbg_document.h"
#include "hp_data_reader.h"

namespace {

void append_u32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
    bytes->push_back(static_cast<std::uint8_t>(value & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
}

void append_i32(std::vector<std::uint8_t>* bytes, std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_f32(std::vector<std::uint8_t>* bytes, float value) {
    std::uint32_t raw = 0;
    static_assert(sizeof(raw) == sizeof(value));
    std::memcpy(&raw, &value, sizeof(raw));
    append_u32(bytes, raw);
}

void append_empty_string_field(std::vector<std::uint8_t>* bytes) {
    append_i32(bytes, 0);
    bytes->push_back(0);
}

std::vector<std::uint8_t> build_document() {
    std::vector<std::uint8_t> bytes;

    // Fixed header: legacy six-group path, two floats, one primary record.
    append_i32(&bytes, 0x68);
    append_f32(&bytes, 2.0f);
    append_f32(&bytes, 3.0f);
    append_u32(&bytes, 1u);

    // Section A: one minimum-size 0x4c record with three empty framed strings.
    append_i32(&bytes, 11);
    for (int i = 0; i < 12; ++i) append_f32(&bytes, static_cast<float>(i + 1));
    bytes.push_back(1);
    append_i32(&bytes, 22);
    append_f32(&bytes, 4.0f);
    append_empty_string_field(&bytes);
    append_empty_string_field(&bytes);
    append_empty_string_field(&bytes);

    // Section B and C: nonpositive/empty blocks.
    append_i32(&bytes, 0);
    append_i32(&bytes, 0);

    // Section D: header_word0 == 0x68 selects exactly six group headers.
    for (int i = 0; i < 6; ++i) append_i32(&bytes, 0);

    // Section E: empty fixed-tail block.
    append_i32(&bytes, 0);

    // Section F: one 24-byte tuple because the primary count is one.
    append_i32(&bytes, 1);
    append_i32(&bytes, 2);
    append_i32(&bytes, 3);
    append_i32(&bytes, 4);
    append_f32(&bytes, 0.25f);
    append_f32(&bytes, 0.5f);

    // Section G: one final int. 1 / 2 => 0.5.
    append_i32(&bytes, 2);

    // The composed parser reports, but does not reject, trailing bytes.
    bytes.push_back(0xde);
    bytes.push_back(0xad);
    return bytes;
}

}  // namespace

int main() {
    using namespace nevergone;

    const std::vector<std::uint8_t> bytes = build_document();
    hp_data::Reader reader(bytes);
    enemy_actions_wbg_document::Document document;
    assert(enemy_actions_wbg_document::parse(reader, &document));

    assert(document.prefix.first_i32 == 0x68);
    assert(document.prefix.action_frame_count == 1u);
    assert(document.primary_records.size() == 1u);
    assert(document.primary_records[0].first_i32 == 11);
    assert(document.primary_records[0].bytes_consumed == 0x4cu);
    assert(document.compact_block.records.empty());
    assert(document.nested_block.groups.empty());
    assert(document.versioned_block.expected_group_count == 6u);
    assert(document.versioned_block.groups.size() == 6u);
    assert(document.fixed_tail_block.records.empty());
    assert(document.combo_block.tuples.size() == 1u);
    assert(document.combo_block.tuples[0].i32_values[3] == 4);
    assert(document.combo_block.tuples[0].float_values[0] == 0.25f);
    assert(document.final_table.entries.size() == 1u);
    assert(document.final_table.entries[0].serialized_value == 2);
    assert(document.final_table.entries[0].reciprocal_value == 0.5f);
    assert(document.bytes_consumed == 156u);
    assert(document.trailing_bytes == 2u);
    assert(document.bytes_consumed + document.trailing_bytes == reader.size());

    std::vector<std::uint8_t> truncated = bytes;
    truncated.resize(155u);
    hp_data::Reader truncated_reader(truncated);
    enemy_actions_wbg_document::Document preserved;
    preserved.bytes_consumed = 77u;
    preserved.trailing_bytes = 88u;
    preserved.primary_records.resize(2u);
    assert(!enemy_actions_wbg_document::parse(truncated_reader, &preserved));
    assert(preserved.bytes_consumed == 77u);
    assert(preserved.trailing_bytes == 88u);
    assert(preserved.primary_records.size() == 2u);

    assert(!enemy_actions_wbg_document::parse(reader, nullptr));
    return 0;
}
