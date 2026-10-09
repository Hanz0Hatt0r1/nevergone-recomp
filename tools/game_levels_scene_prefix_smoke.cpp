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
    bytes.push_back(0x7fu);          // verified one-byte gap
    bytes.insert(bytes.end(), {'h', 'e', 'r', 'o'});
    append_f32(bytes, 1.5f);
    append_f32(bytes, -2.25f);
    append_u32(bytes, 1u);           // layer_count
    append_f32(bytes, 0.75f);
    append_u32(bytes, 1u);           // object_count
    append_u32(bytes, 0xfffffffdu);  // first object opaque int32: -3
    append_u32(bytes, 3u);           // proven object string length
    bytes.push_back(0xaau);          // verified one-byte gap
    bytes.insert(bytes.end(), {'n', 'p', 'c'});
    append_f32(bytes, 10.0f);        // first CCPoint.x
    append_f32(bytes, 20.0f);        // first CCPoint.y
    append_f32(bytes, 30.0f);        // opaque scalar float
    append_f32(bytes, 40.0f);        // second CCPoint.x
    append_f32(bytes, 50.0f);        // second CCPoint.y
    append_u32(bytes, 0xfffffff9u);  // following opaque int32: -7
    bytes.push_back(1u);             // first bool
    bytes.push_back(0u);             // second bool
    bytes.push_back(0xeeu);          // version-dependent bytes remain untouched

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
    assert(object_prefix.string_length == 3u);
    assert(object_prefix.bytes_consumed == 45u);

    FirstObjectHeader object_header;
    assert(parse_first_object_header(reader, &object_header));
    assert(object_header.first_string == "npc");
    assert(std::fabs(object_header.first_point_x - 10.0f) < 0.000001f);
    assert(std::fabs(object_header.first_point_y - 20.0f) < 0.000001f);
    assert(std::fabs(object_header.first_float - 30.0f) < 0.000001f);
    assert(std::fabs(object_header.second_point_x - 40.0f) < 0.000001f);
    assert(std::fabs(object_header.second_point_y - 50.0f) < 0.000001f);
    assert(object_header.second_i32 == -7);
    assert(object_header.first_bool);
    assert(!object_header.second_bool);
    assert(object_header.bytes_consumed == 75u);

    std::vector<std::uint8_t> no_object = bytes;
    no_object[33] = 0u;
    no_object[34] = no_object[35] = no_object[36] = 0u;
    nevergone::hp_data::Reader no_object_reader(no_object);
    assert(parse_first_layer_header(no_object_reader, &layer_header));
    assert(layer_header.object_count == 0u);
    assert(!parse_first_object_prefix(no_object_reader, &object_prefix));
    assert(!parse_first_object_header(no_object_reader, &object_header));

    const std::vector<std::uint8_t> truncated_prefix(bytes.begin(), bytes.begin() + 44);
    nevergone::hp_data::Reader truncated_prefix_reader(truncated_prefix);
    FirstObjectPrefix unchanged_prefix;
    unchanged_prefix.first_i32 = 11;
    unchanged_prefix.string_length = 22;
    unchanged_prefix.bytes_consumed = 33;
    assert(!parse_first_object_prefix(truncated_prefix_reader, &unchanged_prefix));
    assert(unchanged_prefix.first_i32 == 11);
    assert(unchanged_prefix.string_length == 22u);
    assert(unchanged_prefix.bytes_consumed == 33u);

    const std::vector<std::uint8_t> truncated_header(bytes.begin(), bytes.begin() + 74);
    nevergone::hp_data::Reader truncated_header_reader(truncated_header);
    FirstObjectHeader unchanged_header;
    unchanged_header.first_string = "keep";
    unchanged_header.second_i32 = 77;
    unchanged_header.bytes_consumed = 88;
    assert(!parse_first_object_header(truncated_header_reader, &unchanged_header));
    assert(unchanged_header.first_string == "keep");
    assert(unchanged_header.second_i32 == 77);
    assert(unchanged_header.bytes_consumed == 88u);

    std::vector<std::uint8_t> hostile_object_length = bytes;
    hostile_object_length[41] = 0xffu;
    hostile_object_length[42] = 0xffu;
    hostile_object_length[43] = 0xffu;
    hostile_object_length[44] = 0x7fu;
    nevergone::hp_data::Reader hostile_object_reader(hostile_object_length);
    assert(parse_first_object_prefix(hostile_object_reader, &object_prefix));
    assert(!parse_first_object_header(hostile_object_reader, &object_header));

    std::vector<std::uint8_t> no_scene;
    append_u32(no_scene, 7u);
    append_u32(no_scene, 0u);
    nevergone::hp_data::Reader no_scene_reader(no_scene);
    assert(!parse_first_object_prefix(no_scene_reader, &object_prefix));
    assert(!parse_first_object_header(no_scene_reader, &object_header));

    std::vector<std::uint8_t> oversized_scene_length = bytes;
    oversized_scene_length[8] = 0xffu;
    oversized_scene_length[9] = 0xffu;
    oversized_scene_length[10] = 0xffu;
    oversized_scene_length[11] = 0x7fu;
    nevergone::hp_data::Reader oversized_reader(oversized_scene_length);
    assert(!parse_first_object_prefix(oversized_reader, &object_prefix));
    assert(!parse_first_object_header(oversized_reader, &object_header));

    assert(!parse(reader, nullptr));
    assert(!parse_first_scene_header(reader, nullptr));
    assert(!parse_first_layer_header(reader, nullptr));
    assert(!parse_first_object_prefix(reader, nullptr));
    assert(!parse_first_object_header(reader, nullptr));
    return 0;
}
