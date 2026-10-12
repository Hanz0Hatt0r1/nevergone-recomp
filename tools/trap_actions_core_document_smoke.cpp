#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "trap_actions_core_document.h"

namespace trap = nevergone::trap_actions_core_document;

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

std::vector<std::uint8_t> make_empty_core(std::int32_t version_word) {
    std::vector<std::uint8_t> bytes;
    // Trap fixed prefix / primary count zero.
    append_i32(&bytes, version_word);
    append_f32(&bytes, 1.0f);
    append_f32(&bytes, 2.0f);
    append_u32(&bytes, 0u);
    // Trap-specific unsigned compact count.
    append_u32(&bytes, 0u);
    // Nested outer signed group count.
    append_i32(&bytes, 0);
    // Versioned groups: exactly 6 for <=0x68, otherwise 20. Every group has
    // a signed zero record count in this synthetic boundary case.
    const std::size_t groups = version_word > 0x68 ? 20u : 6u;
    for (std::size_t i = 0; i < groups; ++i) append_i32(&bytes, 0);
    // Fixed-tail signed count.
    append_i32(&bytes, 0);
    return bytes;
}

}  // namespace

int main() {
    auto legacy_bytes = make_empty_core(0x68);
    nevergone::hp_data::Reader legacy_reader(legacy_bytes);
    trap::Document legacy;
    assert(trap::parse(legacy_reader, &legacy));
    assert(legacy.primary.prefix.first_i32 == 0x68);
    assert(legacy.primary.records.empty());
    assert(legacy.compact.record_count == 0u);
    assert(legacy.nested.groups.empty());
    assert(legacy.versioned.expected_group_count == 6u);
    assert(legacy.versioned.groups.size() == 6u);
    assert(legacy.fixed_tail.records.empty());
    assert(legacy.bytes_consumed == legacy_bytes.size());
    assert(legacy.trailing_bytes == 0u);
    assert(legacy.bytes_consumed == 52u);

    auto modern_bytes = make_empty_core(0x69);
    modern_bytes.push_back(0xdeu);
    modern_bytes.push_back(0xadu);
    const std::size_t expected_core_size = modern_bytes.size() - 2u;
    nevergone::hp_data::Reader modern_reader(modern_bytes);
    trap::Document modern;
    assert(trap::parse(modern_reader, &modern));
    assert(modern.versioned.expected_group_count == 20u);
    assert(modern.versioned.groups.size() == 20u);
    assert(modern.bytes_consumed == expected_core_size);
    assert(modern.trailing_bytes == 2u);
    assert(modern.bytes_consumed == 108u);

    // Removing any byte from the recovered core boundary must fail
    // transactionally rather than publish a partial document.
    nevergone::hp_data::Reader truncated(legacy_bytes.data(), legacy_bytes.size() - 1u);
    trap::Document unchanged;
    unchanged.bytes_consumed = 777u;
    unchanged.trailing_bytes = 888u;
    assert(!trap::parse(truncated, &unchanged));
    assert(unchanged.bytes_consumed == 777u);
    assert(unchanged.trailing_bytes == 888u);

    return 0;
}
