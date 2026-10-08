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
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}
}  // namespace

int main() {
    using namespace nevergone::game_levels_scene_prefix;

    std::vector<std::uint8_t> bytes;
    append_u32(bytes, 0xfffffffeu);
    append_u32(bytes, 1u);           // scene_count
    append_u32(bytes, 4u);           // scene string length
    bytes.push_back(0x7fu);
    bytes.insert(bytes.end(), {'h', 'e', 'r', 'o'});
    append_f32(bytes, 1.5f);
    append_f32(bytes, -2.25f);
    append_u32(bytes, 1u);           // layer_count
    append_f32(bytes, 0.75f);
    append_u32(bytes, 1u);           // object_count
    append_u32(bytes, 0xfffffffdu);  // first object opaque int32: -3
    append_u32(bytes, 9u);           // first object opaque uint32
    bytes.push_back(0xaau);          // following char-field data untouched

    nevergone::hp_data::Reader reader(bytes);

    Prefix prefix;
    assert(parse(reader, &prefix));
    assert(prefix.first_i32 == -2);
    assert(prefix.scene_count == 1u);
    assert(prefix.bytes_consumed == 8u);

    FirstSceneHeader scene_header;
    assert(parse_first_scene_header(reader, &scene_header));
    assert(scene_header.first_string == "hero");
    assert(std::fabs(scene_header.first_point_x - 1.5f) < 0.000001f);
    assert(std::fabs(scene_header.first_point_y + 2.25f) < 0.000001f);
    assert(scene_header.layer_count == 1u);
    assert(scene_header.bytes_consumed == 29u);

    FirstLayerHeader layer_header;
    assert(parse_first_layer_header(reader, &layer_header));
    assert(std::fabs(layer_header.first_float - 0.75f) < 0.000001f);
    assert(layer_header.object_count == 1u);
    assert(layer_header.bytes_consumed == 37u);

    FirstObjectPrefix object_prefix;
    assert(parse_first_object_prefix(reader, &object_prefix));
    assert(object_prefix.first_i32 == -3);
    assert(object_prefix.second_u32 == 9u);
    assert(object_prefix.bytes_consumed == 45u);

    std::vector<std::uint8_t> no_object = bytes;
    no_object[33] = 0u;
    no_object[34] = no_object[35] = no_object[36] = 0u;
    nevergone::hp_data::Reader no_object_reader(no_object);
    assert(parse_first_layer_header(no_object_reader, &layer_header));
    assert(layer_header.object_count == 0u);
    assert(!parse_first_object_prefix(no_object_reader, &object_prefix));

    const std::vector<std::uint8_t> truncated_object(bytes.begin(), bytes.begin() + 44);
    nevergone::hp_data::Reader truncated_object_reader(truncated_object);
    FirstObjectPrefix unchanged_object;
    unchanged_object.first_i32 = 11;
    unchanged_object.second_u32 = 22;
    unchanged_object.bytes_consumed = 33;
    assert(!parse_first_object_prefix(truncated_object_reader, &unchanged_object));
    assert(unchanged_object.first_i32 == 11);
    assert(unchanged_object.second_u32 == 22u);
    assert(unchanged_object.bytes_consumed == 33u);

    std::vector<std::uint8_t> no_scene;
    append_u32(no_scene, 7u);
    append_u32(no_scene, 0u);
    nevergone::hp_data::Reader no_scene_reader(no_scene);
    assert(!parse_first_object_prefix(no_scene_reader, &object_prefix));

    std::vector<std::uint8_t> oversized_length = bytes;
    oversized_length[8] = 0xff;
    oversized_length[9] = 0xff;
    oversized_length[10] = 0xff;
    oversized_length[11] = 0x7f;
    nevergone::hp_data::Reader oversized_reader(oversized_length);
    assert(!parse_first_object_prefix(oversized_reader, &object_prefix));

    assert(!parse(reader, nullptr));
    assert(!parse_first_scene_header(reader, nullptr));
    assert(!parse_first_layer_header(reader, nullptr));
    assert(!parse_first_object_prefix(reader, nullptr));
    return 0;
}
