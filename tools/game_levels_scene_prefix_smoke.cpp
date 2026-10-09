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
std::vector<std::uint8_t> make_header_through_layer() {
    std::vector<std::uint8_t> bytes;
    append_u32(bytes, 0xfffffffeu);
    append_u32(bytes, 1u);
    append_u32(bytes, 4u);
    bytes.push_back(0x7fu);
    bytes.insert(bytes.end(), {'h', 'e', 'r', 'o'});
    append_f32(bytes, 1.5f);
    append_f32(bytes, -2.25f);
    append_u32(bytes, 1u);
    append_f32(bytes, 0.75f);
    append_u32(bytes, 1u);
    return bytes;
}
std::vector<std::uint8_t> make_object_core_fixture() {
    auto bytes = make_header_through_layer();
    append_u32(bytes, 0xfffffffdu);
    append_u32(bytes, 4u);
    bytes.push_back(0xaau);
    bytes.insert(bytes.end(), {'n', 'o', 'd', 'e'});
    append_f32(bytes, 10.0f);
    append_f32(bytes, -20.0f);
    append_f32(bytes, 0.5f);
    append_f32(bytes, 1.25f);
    append_f32(bytes, -1.5f);
    append_u32(bytes, 0xfffffffbu);
    bytes.push_back(1u);
    bytes.push_back(0u);
    return bytes;
}
}  // namespace

int main() {
    using namespace nevergone::game_levels_scene_prefix;

    const std::vector<std::uint8_t> core_bytes = make_object_core_fixture();
    nevergone::hp_data::Reader reader(core_bytes);

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
    assert(object_prefix.second_u32 == 4u);
    assert(object_prefix.bytes_consumed == 45u);

    FirstObjectCore object_core;
    assert(parse_first_object_core(reader, &object_core));
    assert(object_core.prefix.first_i32 == -3);
    assert(object_core.prefix.second_u32 == 4u);
    assert(object_core.string_value == "node");
    assert(std::fabs(object_core.first_point_x - 10.0f) < 0.000001f);
    assert(std::fabs(object_core.first_point_y + 20.0f) < 0.000001f);
    assert(std::fabs(object_core.middle_float - 0.5f) < 0.000001f);
    assert(std::fabs(object_core.second_point_x - 1.25f) < 0.000001f);
    assert(std::fabs(object_core.second_point_y + 1.5f) < 0.000001f);
    assert(object_core.trailing_i32 == -5);
    assert(object_core.first_bool);
    assert(!object_core.second_bool);
    assert(object_core.bytes_consumed == 76u);

    // Signed top-level values <= 2 do not contain the version-gated vector.
    FirstObjectVersionExtension old_extension;
    assert(parse_first_object_version_extension(reader, &old_extension));
    assert(old_extension.extra_u32_values.empty());
    assert(old_extension.bytes_consumed == 76u);

    // A top-level value > 2 reads one count followed by exactly count uint32s.
    std::vector<std::uint8_t> versioned_bytes = core_bytes;
    versioned_bytes[0] = 3u;
    versioned_bytes[1] = versioned_bytes[2] = versioned_bytes[3] = 0u;
    append_u32(versioned_bytes, 2u);
    append_u32(versioned_bytes, 0x11223344u);
    append_u32(versioned_bytes, 0xaabbccddu);
    nevergone::hp_data::Reader versioned_reader(versioned_bytes);
    FirstObjectVersionExtension versioned_extension;
    assert(parse_first_object_version_extension(versioned_reader, &versioned_extension));
    assert(versioned_extension.core.bytes_consumed == 76u);
    assert(versioned_extension.extra_u32_values.size() == 2u);
    assert(versioned_extension.extra_u32_values[0] == 0x11223344u);
    assert(versioned_extension.extra_u32_values[1] == 0xaabbccddu);
    assert(versioned_extension.bytes_consumed == 88u);

    std::vector<std::uint8_t> hostile_count = core_bytes;
    hostile_count[0] = 3u;
    hostile_count[1] = hostile_count[2] = hostile_count[3] = 0u;
    append_u32(hostile_count, 0xffffffffu);
    nevergone::hp_data::Reader hostile_count_reader(hostile_count);
    FirstObjectVersionExtension unchanged_extension;
    unchanged_extension.extra_u32_values = {7u};
    unchanged_extension.bytes_consumed = 123u;
    assert(!parse_first_object_version_extension(hostile_count_reader, &unchanged_extension));
    assert(unchanged_extension.extra_u32_values.size() == 1u);
    assert(unchanged_extension.extra_u32_values[0] == 7u);
    assert(unchanged_extension.bytes_consumed == 123u);

    std::vector<std::uint8_t> no_object = core_bytes;
    no_object[33] = 0u;
    no_object[34] = no_object[35] = no_object[36] = 0u;
    nevergone::hp_data::Reader no_object_reader(no_object);
    assert(parse_first_layer_header(no_object_reader, &layer_header));
    assert(layer_header.object_count == 0u);
    assert(!parse_first_object_prefix(no_object_reader, &object_prefix));
    assert(!parse_first_object_core(no_object_reader, &object_core));
    assert(!parse_first_object_version_extension(no_object_reader, &old_extension));

    const std::vector<std::uint8_t> truncated_prefix(core_bytes.begin(), core_bytes.begin() + 44);
    nevergone::hp_data::Reader truncated_prefix_reader(truncated_prefix);
    FirstObjectPrefix unchanged_prefix;
    unchanged_prefix.first_i32 = 11;
    unchanged_prefix.second_u32 = 22;
    unchanged_prefix.bytes_consumed = 33;
    assert(!parse_first_object_prefix(truncated_prefix_reader, &unchanged_prefix));
    assert(unchanged_prefix.first_i32 == 11);
    assert(unchanged_prefix.second_u32 == 22u);
    assert(unchanged_prefix.bytes_consumed == 33u);

    const std::vector<std::uint8_t> truncated_core(core_bytes.begin(), core_bytes.end() - 1);
    nevergone::hp_data::Reader truncated_core_reader(truncated_core);
    FirstObjectCore unchanged_core;
    unchanged_core.string_value = "unchanged";
    unchanged_core.trailing_i32 = 99;
    unchanged_core.bytes_consumed = 123;
    assert(!parse_first_object_core(truncated_core_reader, &unchanged_core));
    assert(unchanged_core.string_value == "unchanged");
    assert(unchanged_core.trailing_i32 == 99);
    assert(unchanged_core.bytes_consumed == 123u);

    std::vector<std::uint8_t> hostile_object_length = core_bytes;
    hostile_object_length[41] = 0xffu;
    hostile_object_length[42] = 0xffu;
    hostile_object_length[43] = 0xffu;
    hostile_object_length[44] = 0x7fu;
    nevergone::hp_data::Reader hostile_object_reader(hostile_object_length);
    assert(parse_first_object_prefix(hostile_object_reader, &object_prefix));
    assert(!parse_first_object_core(hostile_object_reader, &object_core));

    std::vector<std::uint8_t> no_scene;
    append_u32(no_scene, 7u);
    append_u32(no_scene, 0u);
    nevergone::hp_data::Reader no_scene_reader(no_scene);
    assert(!parse_first_object_core(no_scene_reader, &object_core));

    std::vector<std::uint8_t> oversized_scene_length = core_bytes;
    oversized_scene_length[8] = 0xff;
    oversized_scene_length[9] = 0xff;
    oversized_scene_length[10] = 0xff;
    oversized_scene_length[11] = 0x7f;
    nevergone::hp_data::Reader oversized_reader(oversized_scene_length);
    assert(!parse_first_object_core(oversized_reader, &object_core));

    assert(!parse(reader, nullptr));
    assert(!parse_first_scene_header(reader, nullptr));
    assert(!parse_first_layer_header(reader, nullptr));
    assert(!parse_first_object_prefix(reader, nullptr));
    assert(!parse_first_object_core(reader, nullptr));
    assert(!parse_first_object_version_extension(reader, nullptr));
    return 0;
}
