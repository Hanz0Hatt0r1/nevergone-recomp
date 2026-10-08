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
    using nevergone::game_levels_scene_prefix::FirstLayerHeader;
    using nevergone::game_levels_scene_prefix::FirstSceneHeader;
    using nevergone::game_levels_scene_prefix::Prefix;
    using nevergone::game_levels_scene_prefix::parse;
    using nevergone::game_levels_scene_prefix::parse_first_layer_header;
    using nevergone::game_levels_scene_prefix::parse_first_scene_header;

    std::vector<std::uint8_t> bytes;
    append_u32(bytes, 0xfffffffeu);  // unresolved signed header field: -2
    append_u32(bytes, 1u);           // scene_count
    append_u32(bytes, 4u);           // first scene char payload length
    bytes.push_back(0x7fu);          // recovered one-byte gap, semantics unresolved
    bytes.insert(bytes.end(), {'h', 'e', 'r', 'o'});
    append_f32(bytes, 1.5f);
    append_f32(bytes, -2.25f);
    append_u32(bytes, 1u);           // layer_count
    append_f32(bytes, 0.75f);        // first layer field, semantics unresolved
    append_u32(bytes, 2u);           // object_count
    bytes.push_back(0xaau);          // later object data must remain untouched

    nevergone::hp_data::Reader reader(bytes);

    Prefix prefix;
    prefix.first_i32 = 77;
    prefix.scene_count = 88;
    prefix.bytes_consumed = 99;
    assert(parse(reader, &prefix));
    assert(prefix.first_i32 == -2);
    assert(prefix.scene_count == 1u);
    assert(prefix.bytes_consumed == 8u);

    FirstSceneHeader scene_header;
    assert(parse_first_scene_header(reader, &scene_header));
    assert(scene_header.prefix.first_i32 == -2);
    assert(scene_header.prefix.scene_count == 1u);
    assert(scene_header.first_string_length == 4u);
    assert(scene_header.first_string == "hero");
    assert(std::fabs(scene_header.first_point_x - 1.5f) < 0.000001f);
    assert(std::fabs(scene_header.first_point_y + 2.25f) < 0.000001f);
    assert(scene_header.layer_count == 1u);
    assert(scene_header.bytes_consumed == 29u);

    FirstLayerHeader layer_header;
    assert(parse_first_layer_header(reader, &layer_header));
    assert(layer_header.scene_header.layer_count == 1u);
    assert(std::fabs(layer_header.first_float - 0.75f) < 0.000001f);
    assert(layer_header.object_count == 2u);
    assert(layer_header.bytes_consumed == 37u);  // scene header + float + object count

    std::vector<std::uint8_t> no_scene_bytes;
    append_u32(no_scene_bytes, 7u);
    append_u32(no_scene_bytes, 0u);
    nevergone::hp_data::Reader no_scene_reader(no_scene_bytes);
    Prefix no_scene_prefix;
    assert(parse(no_scene_reader, &no_scene_prefix));
    assert(no_scene_prefix.scene_count == 0u);
    assert(no_scene_prefix.bytes_consumed == 8u);
    assert(!parse_first_scene_header(no_scene_reader, &scene_header));
    assert(!parse_first_layer_header(no_scene_reader, &layer_header));

    std::vector<std::uint8_t> no_layer_bytes;
    append_u32(no_layer_bytes, 1u);
    append_u32(no_layer_bytes, 1u);
    append_u32(no_layer_bytes, 0u);   // zero-length string
    no_layer_bytes.push_back(0u);     // skipped opaque byte
    append_f32(no_layer_bytes, 0.0f);
    append_f32(no_layer_bytes, 0.0f);
    append_u32(no_layer_bytes, 0u);   // layer_count
    nevergone::hp_data::Reader no_layer_reader(no_layer_bytes);
    assert(parse_first_scene_header(no_layer_reader, &scene_header));
    assert(scene_header.layer_count == 0u);
    assert(!parse_first_layer_header(no_layer_reader, &layer_header));

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

    const std::vector<std::uint8_t> truncated_scene(bytes.begin(), bytes.begin() + 28);
    nevergone::hp_data::Reader scene_reader(truncated_scene);
    FirstSceneHeader unchanged_scene;
    unchanged_scene.first_string = "unchanged";
    unchanged_scene.layer_count = 44;
    unchanged_scene.bytes_consumed = 55;
    assert(!parse_first_scene_header(scene_reader, &unchanged_scene));
    assert(unchanged_scene.first_string == "unchanged");
    assert(unchanged_scene.layer_count == 44u);
    assert(unchanged_scene.bytes_consumed == 55u);

    const std::vector<std::uint8_t> truncated_layer(bytes.begin(), bytes.begin() + 36);
    nevergone::hp_data::Reader layer_reader(truncated_layer);
    FirstLayerHeader unchanged_layer;
    unchanged_layer.first_float = 6.5f;
    unchanged_layer.object_count = 66;
    unchanged_layer.bytes_consumed = 77;
    assert(!parse_first_layer_header(layer_reader, &unchanged_layer));
    assert(std::fabs(unchanged_layer.first_float - 6.5f) < 0.000001f);
    assert(unchanged_layer.object_count == 66u);
    assert(unchanged_layer.bytes_consumed == 77u);

    std::vector<std::uint8_t> oversized_length = bytes;
    oversized_length[8] = 0xff;
    oversized_length[9] = 0xff;
    oversized_length[10] = 0xff;
    oversized_length[11] = 0x7f;
    nevergone::hp_data::Reader oversized_reader(oversized_length);
    assert(!parse_first_scene_header(oversized_reader, &scene_header));
    assert(!parse_first_layer_header(oversized_reader, &layer_header));

    assert(!parse(reader, nullptr));
    assert(!parse_first_scene_header(reader, nullptr));
    assert(!parse_first_layer_header(reader, nullptr));
    return 0;
}
