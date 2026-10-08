#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "hp_data_reader.h"

int main() {
    const std::vector<std::uint8_t> bytes = {
            0x78, 0x56, 0x34, 0x12,
            0x00, 0x00, 0x80, 0x3f,
            0x01, 0x00,
            'h', 'e', 'r', 'o', 0x00, 'x', 'x', 'x',
            'f', 'u', 'l', 'l',
    };
    const nevergone::hp_data::Reader reader(bytes);
    assert(reader.size() == bytes.size());

    std::uint32_t u32 = 0;
    assert(reader.read_u32_le(0, &u32));
    assert(u32 == 0x12345678u);

    std::int32_t i32 = 0;
    assert(reader.read_i32_le(0, &i32));
    assert(i32 == 0x12345678);

    float f32 = 0.0f;
    assert(reader.read_f32_le(4, &f32));
    assert(std::fabs(f32 - 1.0f) < 0.000001f);

    bool flag = false;
    assert(reader.read_bool8(8, &flag));
    assert(flag);
    assert(reader.read_bool8(9, &flag));
    assert(!flag);

    std::uint8_t copy[4] = {};
    assert(reader.read_bytes(0, sizeof(copy), copy));
    assert(copy[0] == 0x78 && copy[3] == 0x12);

    std::string text;
    assert(reader.read_fixed_string(10, 8, &text));
    assert(text == "hero");
    assert(reader.read_fixed_string(18, 4, &text));
    assert(text == "full");
    assert(reader.read_fixed_string(bytes.size(), 0, &text));
    assert(text.empty());

    const std::size_t short_tail = bytes.size() - 3;
    assert(!reader.read_u32_le(short_tail, &u32));
    assert(!reader.read_f32_le(short_tail, &f32));
    assert(!reader.read_bool8(bytes.size(), &flag));
    assert(!reader.read_bytes(bytes.size() - 1, 2, copy));
    assert(!reader.read_bytes(0, 1, nullptr));
    assert(reader.read_bytes(bytes.size(), 0, nullptr));
    assert(!reader.read_fixed_string(bytes.size() - 1, 2, &text));
    assert(!reader.read_fixed_string(0, 1, nullptr));
    return 0;
}
