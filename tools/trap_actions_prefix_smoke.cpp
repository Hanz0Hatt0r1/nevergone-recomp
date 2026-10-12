#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "trap_actions_prefix.h"

namespace trap = nevergone::trap_actions_prefix;

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
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

}  // namespace

int main() {
    std::vector<std::uint8_t> bytes;
    append_i32(&bytes, -7);
    append_f32(&bytes, 1.25f);
    append_f32(&bytes, -2.5f);
    append_u32(&bytes, 3u);

    nevergone::hp_data::Reader reader(bytes);
    trap::Prefix prefix;
    assert(trap::parse(reader, &prefix));
    assert(prefix.first_i32 == -7);
    assert(prefix.first_float == 1.25f);
    assert(prefix.second_float == -2.5f);
    assert(prefix.record_count == 3u);
    assert(prefix.bytes_consumed == 16u);

    // The parser must fail transactionally on every truncated fixed prefix.
    for (std::size_t size = 0; size < 16u; ++size) {
        nevergone::hp_data::Reader truncated(bytes.data(), size);
        trap::Prefix unchanged;
        unchanged.first_i32 = 123;
        unchanged.first_float = 4.0f;
        unchanged.second_float = 5.0f;
        unchanged.record_count = 6u;
        unchanged.bytes_consumed = 7u;
        assert(!trap::parse(truncated, &unchanged));
        assert(unchanged.first_i32 == 123);
        assert(unchanged.first_float == 4.0f);
        assert(unchanged.second_float == 5.0f);
        assert(unchanged.record_count == 6u);
        assert(unchanged.bytes_consumed == 7u);
    }

    std::vector<std::uint8_t> nan_bytes;
    append_i32(&nan_bytes, 1);
    append_f32(&nan_bytes, std::numeric_limits<float>::quiet_NaN());
    append_f32(&nan_bytes, 0.0f);
    append_u32(&nan_bytes, 1u);
    nevergone::hp_data::Reader nan_reader(nan_bytes);
    trap::Prefix invalid;
    assert(!trap::parse(nan_reader, &invalid));

    return 0;
}
