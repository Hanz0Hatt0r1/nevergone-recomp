#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "trap_actions_document.h"

namespace trap = nevergone::trap_actions_document;

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
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

void append_string(std::vector<std::uint8_t>* bytes, const std::string& value, std::uint8_t framing) {
    append_i32(bytes, static_cast<std::int32_t>(value.size()));
    bytes->push_back(framing);
    bytes->insert(bytes->end(), value.begin(), value.end());
}

void append_primary_record(std::vector<std::uint8_t>* bytes) {
    append_i32(bytes, 10);
    for (int index = 0; index < 12; ++index) append_f32(bytes, index + 0.25f);
    bytes->push_back(1u);
    append_i32(bytes, 77);
    append_f32(bytes, 3.5f);
    append_string(bytes, "trap.plist", 0x11u);
    append_string(bytes, "aux", 0x22u);
    append_string(bytes, "trap_01.png", 0x33u);
}

std::vector<std::uint8_t> make_document() {
    std::vector<std::uint8_t> bytes;
    append_i32(&bytes, 0x68);
    append_f32(&bytes, 1.0f);
    append_f32(&bytes, 2.0f);
    append_u32(&bytes, 1u);
    append_primary_record(&bytes);

    append_u32(&bytes, 0u);
    append_i32(&bytes, 0);
    for (int i = 0; i < 6; ++i) append_i32(&bytes, 0);
    append_i32(&bytes, 0);

    append_i32(&bytes, 3);
    append_i32(&bytes, 5);
    append_i32(&bytes, 7);
    append_i32(&bytes, 11);
    append_f32(&bytes, 1.25f);
    append_f32(&bytes, 2.5f);

    append_i32(&bytes, 4);
    return bytes;
}

}  // namespace

int main() {
    auto bytes = make_document();
    nevergone::hp_data::Reader reader(bytes);
    trap::Document document;
    assert(trap::parse(reader, &document));
    assert(document.core.primary.prefix.record_count == 1u);
    assert(document.core.primary.records.size() == 1u);
    assert(document.combo.tuples.size() == 1u);

    // The combo parser's derived values are run-boundary records, not copies of
    // the current serialized tuple fields. With one tuple, the last-tuple rule
    // closes the initial run, so both +0x94/+0x98 begin at boundary index 0 and
    // retain initial state 1. +0x90 still carries third-int + 1 per tuple.
    assert(document.combo.array_90_values.size() == 1u);
    assert(document.combo.array_90_values[0].field_1c == 8);
    assert(document.combo.array_94_values.size() == 1u);
    assert(document.combo.array_94_values[0].field_14 == 0);
    assert(document.combo.array_94_values[0].field_1c == 1);
    assert(document.combo.array_98_values.size() == 1u);
    assert(document.combo.array_98_values[0].field_14 == 0);
    assert(document.combo.array_98_values[0].field_1c == 1);

    assert(document.final_table.entries.size() == 1u);
    assert(document.final_table.entries[0].serialized_value == 4);
    assert(document.final_table.entries[0].reciprocal_value == 0.25f);
    assert(document.bytes_consumed == bytes.size());
    assert(document.trailing_bytes == 0u);

    auto with_trailing = bytes;
    with_trailing.push_back(0xdeu);
    with_trailing.push_back(0xadu);
    nevergone::hp_data::Reader trailing_reader(with_trailing);
    trap::Document trailing;
    assert(trap::parse(trailing_reader, &trailing));
    assert(trailing.bytes_consumed == bytes.size());
    assert(trailing.trailing_bytes == 2u);

    nevergone::hp_data::Reader truncated(bytes.data(), bytes.size() - 1u);
    trap::Document unchanged;
    unchanged.bytes_consumed = 777u;
    unchanged.trailing_bytes = 888u;
    assert(!trap::parse(truncated, &unchanged));
    assert(unchanged.bytes_consumed == 777u);
    assert(unchanged.trailing_bytes == 888u);

    return 0;
}
