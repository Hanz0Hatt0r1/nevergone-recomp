#include <cassert>
#include <cstdint>
#include <vector>

#include "game_levels_scene_prefix.h"
#include "hp_data_reader.h"

int main() {
    using nevergone::game_levels_scene_prefix::Prefix;
    using nevergone::game_levels_scene_prefix::parse;

    const std::vector<std::uint8_t> bytes = {
        0xfe, 0xff, 0xff, 0xff,  // -2
        0x78, 0x56, 0x34, 0x12,  // 0x12345678
        0xef, 0xcd, 0xab, 0x90,  // 0x90abcdef
        0xaa, 0xbb,
    };
    nevergone::hp_data::Reader reader(bytes);

    Prefix prefix;
    prefix.first_i32 = 77;
    prefix.second_u32 = 88;
    prefix.third_u32 = 99;
    prefix.bytes_consumed = 111;
    assert(parse(reader, &prefix));
    assert(prefix.first_i32 == -2);
    assert(prefix.second_u32 == 0x12345678u);
    assert(prefix.third_u32 == 0x90abcdefu);
    assert(prefix.bytes_consumed == 12u);

    const std::vector<std::uint8_t> truncated_bytes = {
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00,
    };
    nevergone::hp_data::Reader truncated(truncated_bytes);
    Prefix unchanged;
    unchanged.first_i32 = 11;
    unchanged.second_u32 = 22;
    unchanged.third_u32 = 33;
    unchanged.bytes_consumed = 44;
    assert(!parse(truncated, &unchanged));
    assert(unchanged.first_i32 == 11);
    assert(unchanged.second_u32 == 22u);
    assert(unchanged.third_u32 == 33u);
    assert(unchanged.bytes_consumed == 44u);

    assert(!parse(reader, nullptr));
    return 0;
}
