#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "trap_actions_compact.h"

namespace trap = nevergone::trap_actions_compact;

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

}  // namespace

int main() {
    std::vector<std::uint8_t> bytes(9u, 0xa5u);
    const std::size_t start = bytes.size();
    append_u32(&bytes, 2u);
    append_i32(&bytes, 17);
    append_f32(&bytes, 1.5f);
    append_f32(&bytes, -2.25f);
    append_i32(&bytes, -3);
    append_f32(&bytes, 4.0f);
    append_f32(&bytes, 9.75f);

    nevergone::hp_data::Reader reader(bytes);
    trap::Block block;
    assert(trap::parse(reader, start, &block));
    assert(block.record_count == 2u);
    assert(block.records.size() == 2u);
    assert(block.records[0].first_i32 == 17);
    assert(block.records[0].first_float == 1.5f);
    assert(block.records[0].second_float == -2.25f);
    assert(block.records[1].first_i32 == -3);
    assert(block.records[1].first_float == 4.0f);
    assert(block.records[1].second_float == 9.75f);
    assert(block.bytes_consumed == 4u + 2u * 12u);

    // Zero is a valid unsigned count and consumes exactly its header.
    std::vector<std::uint8_t> empty_bytes;
    append_u32(&empty_bytes, 0u);
    nevergone::hp_data::Reader empty_reader(empty_bytes);
    trap::Block empty;
    assert(trap::parse(empty_reader, 0u, &empty));
    assert(empty.record_count == 0u);
    assert(empty.records.empty());
    assert(empty.bytes_consumed == 4u);

    // The native loop is unsigned. A high-bit count is not a negative/empty
    // sentinel and must fail when the declared records cannot fit the buffer.
    std::vector<std::uint8_t> high_count;
    append_u32(&high_count, 0x80000000u);
    nevergone::hp_data::Reader high_reader(high_count);
    trap::Block unchanged;
    unchanged.record_count = 7u;
    unchanged.bytes_consumed = 99u;
    assert(!trap::parse(high_reader, 0u, &unchanged));
    assert(unchanged.record_count == 7u);
    assert(unchanged.bytes_consumed == 99u);

    // Truncation of a declared record is transactional.
    nevergone::hp_data::Reader truncated(bytes.data(), bytes.size() - 1u);
    trap::Block truncated_out;
    truncated_out.bytes_consumed = 55u;
    assert(!trap::parse(truncated, start, &truncated_out));
    assert(truncated_out.records.empty());
    assert(truncated_out.bytes_consumed == 55u);

    assert(!trap::parse(reader, bytes.size() + 1u, &block));
    return 0;
}
