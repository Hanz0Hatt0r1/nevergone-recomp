#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "game_levels_scene_prefix.h"
#include "hp_data_reader.h"

namespace {

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
}

void append_f32(std::vector<std::uint8_t>& bytes, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

}  // namespace

int main() {
    using nevergone::game_levels_scene_prefix::FirstRecordHeader;
    using nevergone::game_levels_scene_prefix::Prefix;
    using nevergone::game_levels_scene_prefix::parse;
    using nevergone::game_levels_scene_prefix::parse_first_record_header;

    std::vector<std::uint8_t> bytes;
    append_u32(bytes, 0xfffffffeu);  // -2
    append_u32(bytes, 0x12345678u);
    append_u32(bytes, 4u);           // first char payload length
    bytes.push_back(0x7fu);          // recovered one-byte gap, semantics unresolved
    bytes.insert(bytes.end(), {'h', 'e', 'r', 'o'});
    append_f32(bytes, 1.5f);
    append_f32(bytes, -2.25f);
    bytes.push_back(0xaau);          // later data must remain untouched

    nevergone::hp_data::Reader reader(bytes);

    Prefix prefix;
    prefix.first_i32 = 77;
    prefix.second_u32 = 88;
    prefix.third_u32 = 99;
    prefix.bytes_consumed = 111;
    assert(parse(reader, &prefix));
    assert(prefix.first_i32 == -2);
    assert(prefix.second_u32 == 0x12345678u);
    assert(prefix.third_u32 == 4u);
    assert(prefix.bytes_consumed == 12u);

    FirstRecordHeader header;
    assert(parse_first_record_header(reader, &header));
    assert(header.prefix.first_i32 == -2);
    assert(header.prefix.second_u32 == 0x12345678u);
    assert(header.prefix.third_u32 == 4u);
    assert(header.first_string == "hero");
    assert(std::fabs(header.first_point_x - 1.5f) < 0.000001f);
    assert(std::fabs(header.first_point_y + 2.25f) < 0.000001f);
    assert(header.bytes_consumed == 25u);  // 12 + 1 + 4 + 4 + 4

    const std::vector<std::uint8_t> truncated_prefix(bytes.begin(), bytes.begin() + 11);
    nevergone::hp_data::Reader prefix_reader(truncated_prefix);
    Prefix unchanged;
    unchanged.first_i32 = 11;
    unchanged.second_u32 = 22;
    unchanged.third_u32 = 33;
    unchanged.bytes_consumed = 44;
    assert(!parse(prefix_reader, &unchanged));
    assert(unchanged.first_i32 == 11);
    assert(unchanged.second_u32 == 22u);
    assert(unchanged.third_u32 == 33u);
    assert(unchanged.bytes_consumed == 44u);

    const std::vector<std::uint8_t> truncated_header(bytes.begin(), bytes.begin() + 24);
    nevergone::hp_data::Reader header_reader(truncated_header);
    FirstRecordHeader unchanged_header;
    unchanged_header.first_string = "unchanged";
    unchanged_header.bytes_consumed = 55;
    assert(!parse_first_record_header(header_reader, &unchanged_header));
    assert(unchanged_header.first_string == "unchanged");
    assert(unchanged_header.bytes_consumed == 55u);

    std::vector<std::uint8_t> oversized_length = bytes;
    oversized_length[8] = 0xff;
    oversized_length[9] = 0xff;
    oversized_length[10] = 0xff;
    oversized_length[11] = 0x7f;
    nevergone::hp_data::Reader oversized_reader(oversized_length);
    assert(!parse_first_record_header(oversized_reader, &header));

    assert(!parse(reader, nullptr));
    assert(!parse_first_record_header(reader, nullptr));
    return 0;
}
