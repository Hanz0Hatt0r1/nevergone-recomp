#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "trap_actions_primary.h"

namespace trap = nevergone::trap_actions_primary;

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

void append_record(
        std::vector<std::uint8_t>* bytes,
        std::int32_t state,
        const std::string& atlas,
        const std::string& second,
        const std::string& frame) {
    append_i32(bytes, state);
    for (int index = 0; index < 12; ++index) {
        append_f32(bytes, static_cast<float>(index) + 0.25f);
    }
    bytes->push_back(1u);
    append_i32(bytes, 77);
    append_f32(bytes, 3.5f);
    append_string(bytes, atlas, 0x11u);
    append_string(bytes, second, 0x22u);
    append_string(bytes, frame, 0x33u);
}

}  // namespace

int main() {
    std::vector<std::uint8_t> bytes;
    append_i32(&bytes, 0x68);
    append_f32(&bytes, 1.0f);
    append_f32(&bytes, 2.0f);
    append_u32(&bytes, 2u);
    append_record(&bytes, 10, "scene_a.plist", "aux-a", "scene_a_01.png");
    append_record(&bytes, 11, "scene_b.plist", "aux-b", "scene_b_02.png");

    nevergone::hp_data::Reader reader(bytes);
    trap::Block block;
    assert(trap::parse(reader, &block));
    assert(block.prefix.first_i32 == 0x68);
    assert(block.prefix.record_count == 2u);
    assert(block.records.size() == 2u);
    assert(block.bytes_consumed == bytes.size());

    const auto& first = block.records[0];
    assert(first.first_i32 == 10);
    assert(first.float_values[0] == 0.25f);
    assert(first.float_values[11] == 11.25f);
    assert(first.first_bool);
    assert(first.second_i32 == 77);
    assert(first.trailing_float == 3.5f);
    assert(first.first_string == "scene_a.plist");
    assert(first.second_string == "aux-a");
    assert(first.third_string == "scene_a_01.png");
    assert(first.first_string_framing_u8 == 0x11u);
    assert(first.second_string_framing_u8 == 0x22u);
    assert(first.third_string_framing_u8 == 0x33u);

    const auto& second = block.records[1];
    assert(second.first_i32 == 11);
    assert(second.first_string == "scene_b.plist");
    assert(second.third_string == "scene_b_02.png");

    // Declared count must be satisfied transactionally; truncating the second
    // record cannot publish a partially parsed primary array.
    nevergone::hp_data::Reader truncated(bytes.data(), bytes.size() - 1u);
    trap::Block unchanged;
    unchanged.bytes_consumed = 123u;
    assert(!trap::parse(truncated, &unchanged));
    assert(unchanged.records.empty());
    assert(unchanged.bytes_consumed == 123u);

    // An impossible declared count is rejected before reserve/loop expansion.
    std::vector<std::uint8_t> hostile(bytes.begin(), bytes.begin() + 16u);
    hostile[12] = 0xffu;
    hostile[13] = 0xffu;
    hostile[14] = 0xffu;
    hostile[15] = 0x7fu;
    nevergone::hp_data::Reader hostile_reader(hostile);
    trap::Block rejected;
    assert(!trap::parse(hostile_reader, &rejected));

    return 0;
}
