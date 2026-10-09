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
void append_i32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}
void append_f32(std::vector<std::uint8_t>& bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}
void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    assert(offset + 4u <= bytes.size());
    for (std::size_t i = 0; i < 4u; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
    }
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
    append_i32(bytes, -3);
    append_u32(bytes, 4u);
    bytes.push_back(0xaau);
    bytes.insert(bytes.end(), {'n', 'o', 'd', 'e'});
    append_f32(bytes, 10.0f);
    append_f32(bytes, -20.0f);
    append_f32(bytes, 0.5f);
    append_f32(bytes, 1.25f);
    append_f32(bytes, -1.5f);
    append_i32(bytes, -5);
    bytes.push_back(1u);
    bytes.push_back(0u);
    assert(bytes.size() == 76u);
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

    FirstObjectVersionExtension old_extension;
    assert(parse_first_object_version_extension(reader, &old_extension));
    assert(old_extension.extra_u32_values.empty());
    assert(old_extension.bytes_consumed == 76u);

    std::vector<std::uint8_t> zero_object = core_bytes;
    write_u32(zero_object, 37u, 0u);
    nevergone::hp_data::Reader zero_object_reader(zero_object);
    FirstObjectConditionalHeader zero_header;
    assert(parse_first_object_conditional_header(zero_object_reader, &zero_header));
    assert(!zero_header.present);
    assert(zero_header.bytes_consumed == 76u);
    FirstObjectRecord zero_record;
    assert(parse_first_object_record(zero_object_reader, &zero_record));
    assert(!zero_record.has_tail_i32);
    assert(!zero_record.has_tail_points);
    assert(zero_record.bytes_consumed == 76u);

    std::vector<std::uint8_t> v1 = core_bytes;
    write_u32(v1, 0u, 1u);
    write_u32(v1, 37u, 1u);
    append_u32(v1, 0x11111111u);
    append_u32(v1, 3u);
    v1.push_back(0x55u);
    v1.insert(v1.end(), {'f', 'o', 'o'});
    append_i32(v1, 1);
    append_i32(v1, -9);
    assert(v1.size() == 96u);
    nevergone::hp_data::Reader v1_reader(v1);
    FirstObjectConditionalHeader v1_header;
    assert(parse_first_object_conditional_header(v1_reader, &v1_header));
    assert(v1_header.first_u32 == 0x11111111u);
    assert(v1_header.second_u32 == 0u);
    assert(v1_header.string_value == "foo");
    assert(v1_header.primary_i32 == 1);
    assert(v1_header.secondary_i32 == -9);
    FirstObjectRecord v1_record;
    assert(parse_first_object_record(v1_reader, &v1_record));
    assert(!v1_record.has_tail_i32);
    assert(v1_record.bytes_consumed == 96u);

    std::vector<std::uint8_t> v2 = core_bytes;
    write_u32(v2, 0u, 2u);
    write_u32(v2, 37u, 1u);
    append_u32(v2, 0x22222221u);
    append_u32(v2, 0x22222222u);
    append_u32(v2, 3u);
    v2.push_back(0x66u);
    v2.insert(v2.end(), {'b', 'a', 'r'});
    append_i32(v2, 0);
    assert(v2.size() == 96u);
    nevergone::hp_data::Reader v2_reader(v2);
    FirstObjectConditionalHeader v2_header;
    assert(parse_first_object_conditional_header(v2_reader, &v2_header));
    assert(v2_header.first_u32 == 0x22222221u);
    assert(v2_header.second_u32 == 0x22222222u);
    assert(v2_header.string_value == "bar");
    assert(v2_header.bytes_consumed == 96u);

    std::vector<std::uint8_t> v3 = core_bytes;
    write_u32(v3, 0u, 3u);
    write_u32(v3, 37u, 1u);
    append_u32(v3, 1u);
    append_u32(v3, 0xdeadbeefu);
    append_u32(v3, 0x33333331u);
    append_u32(v3, 0x33333332u);
    append_u32(v3, 3u);
    v3.push_back(0x77u);
    v3.insert(v3.end(), {'b', 'a', 'z'});
    append_i32(v3, 1);
    append_i32(v3, -7);
    assert(v3.size() == 108u);
    nevergone::hp_data::Reader v3_reader(v3);
    FirstObjectConditionalHeader v3_header;
    assert(parse_first_object_conditional_header(v3_reader, &v3_header));
    assert(v3_header.extension.extra_u32_values.size() == 1u);
    assert(v3_header.extension.extra_u32_values[0] == 0xdeadbeefu);
    assert(v3_header.bytes_consumed == 108u);

    // Types 4 and 6 share the same BIC/cmp branch and consume one int32.
    for (std::int32_t object_type : {4, 6}) {
        std::vector<std::uint8_t> typed = v2;
        write_u32(typed, 37u, static_cast<std::uint32_t>(object_type));
        append_i32(typed, 40 + object_type);
        nevergone::hp_data::Reader typed_reader(typed);
        FirstObjectRecord record;
        assert(parse_first_object_record(typed_reader, &record));
        assert(record.has_tail_i32);
        assert(!record.has_tail_points);
        assert(record.tail_i32 == 40 + object_type);
        assert(record.bytes_consumed == 100u);
    }

    std::vector<std::uint8_t> type9 = v2;
    write_u32(type9, 37u, 9u);
    append_i32(type9, -99);
    nevergone::hp_data::Reader type9_reader(type9);
    FirstObjectRecord type9_record;
    assert(parse_first_object_record(type9_reader, &type9_record));
    assert(type9_record.has_tail_i32);
    assert(!type9_record.has_tail_points);
    assert(type9_record.tail_i32 == -99);
    assert(type9_record.bytes_consumed == 100u);

    std::vector<std::uint8_t> type10 = v2;
    write_u32(type10, 37u, 10u);
    append_i32(type10, 123);
    append_f32(type10, 11.0f);
    append_f32(type10, 22.0f);
    append_f32(type10, 33.0f);
    append_f32(type10, 44.0f);
    nevergone::hp_data::Reader type10_reader(type10);
    FirstObjectRecord type10_record;
    assert(parse_first_object_record(type10_reader, &type10_record));
    assert(type10_record.has_tail_i32);
    assert(type10_record.has_tail_points);
    assert(type10_record.tail_i32 == 123);
    assert(std::fabs(type10_record.first_tail_point_x - 11.0f) < 0.000001f);
    assert(std::fabs(type10_record.first_tail_point_y - 22.0f) < 0.000001f);
    assert(std::fabs(type10_record.second_tail_point_x - 33.0f) < 0.000001f);
    assert(std::fabs(type10_record.second_tail_point_y - 44.0f) < 0.000001f);
    assert(type10_record.bytes_consumed == 116u);

    std::vector<std::uint8_t> default_type = v2;
    write_u32(default_type, 37u, 5u);
    nevergone::hp_data::Reader default_type_reader(default_type);
    FirstObjectRecord default_record;
    assert(parse_first_object_record(default_type_reader, &default_record));
    assert(!default_record.has_tail_i32);
    assert(!default_record.has_tail_points);
    assert(default_record.bytes_consumed == 96u);

    std::vector<std::uint8_t> truncated_type10 = v2;
    write_u32(truncated_type10, 37u, 10u);
    append_i32(truncated_type10, 7);
    append_f32(truncated_type10, 1.0f);
    append_f32(truncated_type10, 2.0f);
    append_f32(truncated_type10, 3.0f);
    nevergone::hp_data::Reader truncated_type10_reader(truncated_type10);
    FirstObjectRecord unchanged_record;
    unchanged_record.tail_i32 = 77;
    unchanged_record.bytes_consumed = 555u;
    assert(!parse_first_object_record(truncated_type10_reader, &unchanged_record));
    assert(unchanged_record.tail_i32 == 77);
    assert(unchanged_record.bytes_consumed == 555u);

    std::vector<std::uint8_t> hostile_count = core_bytes;
    write_u32(hostile_count, 0u, 3u);
    append_u32(hostile_count, 0xffffffffu);
    nevergone::hp_data::Reader hostile_count_reader(hostile_count);
    FirstObjectVersionExtension unchanged_extension;
    unchanged_extension.extra_u32_values = {7u};
    unchanged_extension.bytes_consumed = 123u;
    assert(!parse_first_object_version_extension(hostile_count_reader, &unchanged_extension));
    assert(unchanged_extension.extra_u32_values == std::vector<std::uint32_t>{7u});
    assert(unchanged_extension.bytes_consumed == 123u);

    std::vector<std::uint8_t> hostile_string = core_bytes;
    write_u32(hostile_string, 0u, 1u);
    write_u32(hostile_string, 37u, 1u);
    append_u32(hostile_string, 5u);
    append_u32(hostile_string, 0x7fffffffu);
    nevergone::hp_data::Reader hostile_string_reader(hostile_string);
    FirstObjectConditionalHeader unchanged_header;
    unchanged_header.present = true;
    unchanged_header.first_u32 = 9u;
    unchanged_header.string_value = "keep";
    unchanged_header.bytes_consumed = 321u;
    assert(!parse_first_object_conditional_header(hostile_string_reader, &unchanged_header));
    assert(unchanged_header.string_value == "keep");

    std::vector<std::uint8_t> truncated_secondary = core_bytes;
    write_u32(truncated_secondary, 0u, 1u);
    write_u32(truncated_secondary, 37u, 1u);
    append_u32(truncated_secondary, 4u);
    append_u32(truncated_secondary, 1u);
    truncated_secondary.push_back(0x44u);
    truncated_secondary.push_back('x');
    append_i32(truncated_secondary, 1);
    nevergone::hp_data::Reader truncated_secondary_reader(truncated_secondary);
    assert(!parse_first_object_conditional_header(truncated_secondary_reader, &unchanged_header));

    std::vector<std::uint8_t> no_object = core_bytes;
    write_u32(no_object, 33u, 0u);
    nevergone::hp_data::Reader no_object_reader(no_object);
    assert(parse_first_layer_header(no_object_reader, &layer_header));
    assert(layer_header.object_count == 0u);
    assert(!parse_first_object_prefix(no_object_reader, &object_prefix));
    assert(!parse_first_object_core(no_object_reader, &object_core));
    assert(!parse_first_object_version_extension(no_object_reader, &old_extension));
    assert(!parse_first_object_conditional_header(no_object_reader, &zero_header));
    assert(!parse_first_object_record(no_object_reader, &zero_record));

    const std::vector<std::uint8_t> truncated_core(core_bytes.begin(), core_bytes.end() - 1);
    nevergone::hp_data::Reader truncated_core_reader(truncated_core);
    FirstObjectCore unchanged_core;
    unchanged_core.string_value = "unchanged";
    unchanged_core.trailing_i32 = 99;
    unchanged_core.bytes_consumed = 123u;
    assert(!parse_first_object_core(truncated_core_reader, &unchanged_core));
    assert(unchanged_core.string_value == "unchanged");
    assert(unchanged_core.trailing_i32 == 99);
    assert(unchanged_core.bytes_consumed == 123u);

    assert(!parse(reader, nullptr));
    assert(!parse_first_scene_header(reader, nullptr));
    assert(!parse_first_layer_header(reader, nullptr));
    assert(!parse_first_object_prefix(reader, nullptr));
    assert(!parse_first_object_core(reader, nullptr));
    assert(!parse_first_object_version_extension(reader, nullptr));
    assert(!parse_first_object_conditional_header(reader, nullptr));
    assert(!parse_first_object_record(reader, nullptr));
    return 0;
}
