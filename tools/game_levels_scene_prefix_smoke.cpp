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
    using nevergone::game_levels_scene_prefix::FirstSceneHeader;
    using nevergone::game_levels_scene_prefix::Prefix;
    using nevergone::game_levels_scene_prefix::parse;
    using nevergone::game_levels_scene_prefix::parse_first_scene_header;

    std::vector<std::uint8_t> bytes;
    append_u32(bytes, 0xfffffffeu);  // unresolved signed header field: -2
    append_u32(bytes, 1u);           // scene_count
    append_u32(bytes, 4u);           // first scene char payload length
    bytes.push_back(0x7fu);          // recovered one-byte gap, semantics unresolved
    bytes.insert(bytes.end(), {'h', 'e', 'r', 'o'});
    append_f32(bytes, 1.5f);
    append_f32(bytes, -2.25f);
    append_u32(bytes, 3u);           // layer_count
    bytes.push_back(0xaau);          // later data must remain untouched

    nevergone::hp_data::Reader reader(bytes);

    Prefix prefix;
    prefix.first_i32 = 77;
    prefix.scene_count = 88;
    prefix.bytes_consumed = 99;
    assert(parse(reader, &prefix));
    assert(prefix.first_i32 == -2);
    assert(prefix.scene_count == 1u);
    assert(prefix.bytes_consumed == 8u);

    FirstSceneHeader header;
    assert(parse_first_scene_header(reader, &header));
    assert(header.prefix.first_i32 == -2);
    assert(header.prefix.scene_count == 1u);
    assert(header.first_string_length == 4u);
    assert(header.first_string == "hero");
    assert(std::fabs(header.first_point_x - 1.5f) < 0.000001f);
    assert(std::fabs(header.first_point_y + 2.25f) < 0.000001f);
    assert(header.layer_count == 3u);
    assert(header.bytes_consumed == 29u);  // 8 + 4 + 1 + 4 + 4 + 4 + 4

    std::vector<std::uint8_t> no_scene_bytes;
    append_u32(no_scene_bytes, 7u);
    append_u32(no_scene_bytes, 0u);
    nevergone::hp_data::Reader no_scene_reader(no_scene_bytes);
    Prefix no_scene_prefix;
    assert(parse(no_scene_reader, &no_scene_prefix));
    assert(no_scene_prefix.scene_count == 0u);
    assert(no_scene_prefix.bytes_consumed == 8u);
    assert(!parse_first_scene_header(no_scene_reader, &header));

    const std::vector<std::uint8_t> truncated_prefix(bytes.begin(), bytes.begin() + 7);
    nevergone::hp_data::Reader prefix_reader(truncated_prefix);
    Prefix unchanged;
    unchanged.first_i32 = 11;
    unchanged.scene_count = 22;
    unchanged.bytes_consumed = 33;
    assert(!parse(prefix_reader, &unchanged));
    assert(unchanged.first_i32 == 11);
    assert(unchanged.scene_count == 22u);
    assert(unchanged.bytes_consumed == 33u);

    const std::vector<std::uint8_t> truncated_header(bytes.begin(), bytes.begin() + 28);
    nevergone::hp_data::Reader header_reader(truncated_header);
    FirstSceneHeader unchanged_header;
    unchanged_header.first_string = "unchanged";
    unchanged_header.layer_count = 44;
    unchanged_header.bytes_consumed = 55;
    assert(!parse_first_scene_header(header_reader, &unchanged_header));
    assert(unchanged_header.first_string == "unchanged");
    assert(unchanged_header.layer_count == 44u);
    assert(unchanged_header.bytes_consumed == 55u);

    std::vector<std::uint8_t> oversized_length = bytes;
    oversized_length[8] = 0xff;
    oversized_length[9] = 0xff;
    oversized_length[10] = 0xff;
    oversized_length[11] = 0x7f;
    nevergone::hp_data::Reader oversized_reader(oversized_length);
    assert(!parse_first_scene_header(oversized_reader, &header));

    assert(!parse(reader, nullptr));
    assert(!parse_first_scene_header(reader, nullptr));
    return 0;
}
