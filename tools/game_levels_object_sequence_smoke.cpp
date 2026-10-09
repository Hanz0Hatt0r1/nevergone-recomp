#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "game_levels_scene_prefix.h"
#include "hp_data_reader.h"

namespace {
void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void append_i32(std::vector<std::uint8_t>& out, std::int32_t value) { append_u32(out, static_cast<std::uint32_t>(value)); }
void append_f32(std::vector<std::uint8_t>& out, float value) {
    std::uint32_t bits = 0; std::memcpy(&bits, &value, sizeof(bits)); append_u32(out, bits);
}
void write_u32(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value) {
    assert(offset + 4u <= out.size());
    for (std::size_t i = 0; i < 4u; ++i) out[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}
void append_core(std::vector<std::uint8_t>& out, std::int32_t type, const char* text, std::uint32_t length) {
    append_i32(out, type); append_u32(out, length); out.push_back(0xaau);
    out.insert(out.end(), text, text + length);
    append_f32(out, 1.0f); append_f32(out, 2.0f); append_f32(out, 3.0f);
    append_f32(out, 4.0f); append_f32(out, 5.0f);
    append_i32(out, 6); out.push_back(1u); out.push_back(0u);
}
std::vector<std::uint8_t> make_two_object_scene() {
    std::vector<std::uint8_t> out;
    append_i32(out, 2); append_u32(out, 1u);
    append_u32(out, 4u); out.push_back(0x7fu); out.insert(out.end(), {'h','e','r','o'});
    append_f32(out, 1.0f); append_f32(out, 2.0f); append_u32(out, 1u);
    append_f32(out, 0.5f); append_u32(out, 2u);
    assert(out.size() == 37u);
    append_core(out, 0, "a", 1u);
    assert(out.size() == 73u);
    append_core(out, 10, "node", 4u);
    append_u32(out, 11u); append_u32(out, 22u); append_u32(out, 4u);
    out.push_back(0x55u); out.insert(out.end(), {'t','a','i','l'}); append_i32(out, 0);
    append_i32(out, 33);
    append_f32(out, 10.0f); append_f32(out, 20.0f);
    append_f32(out, 30.0f); append_f32(out, 40.0f);
    assert(out.size() == 153u);
    return out;
}
}

int main() {
    namespace scene = nevergone::game_levels_scene_prefix;

    const auto bytes = make_two_object_scene();
    nevergone::hp_data::Reader reader(bytes);

    scene::ObjectRecord first;
    assert(scene::parse_object_record_at(reader, 37u, 2, &first));
    assert(first.start_offset == 37u);
    assert(first.end_offset == 73u);
    assert(first.first_i32 == 0);
    assert(first.string_value == "a");
    assert(!first.conditional_present);

    scene::ObjectRecord second;
    assert(scene::parse_object_record_at(reader, first.end_offset, 2, &second));
    assert(second.start_offset == 73u);
    assert(second.end_offset == 153u);
    assert(second.first_i32 == 10);
    assert(second.string_value == "node");
    assert(second.conditional_present);
    assert(second.conditional_first_u32 == 11u);
    assert(second.conditional_second_u32 == 22u);
    assert(second.conditional_string_value == "tail");
    assert(second.has_tail_i32 && second.tail_i32 == 33);
    assert(second.has_tail_points);
    assert(std::fabs(second.second_tail_point_y - 40.0f) < 0.000001f);

    scene::FirstLayerObjectSequence sequence;
    assert(scene::parse_first_layer_objects(reader, &sequence));
    assert(sequence.layer_header.object_count == 2u);
    assert(sequence.objects.size() == 2u);
    assert(sequence.objects[0].end_offset == sequence.objects[1].start_offset);
    assert(sequence.bytes_consumed == 153u);

    // Empty object loops are complete at the layer-header boundary.
    auto empty_bytes = bytes;
    write_u32(empty_bytes, 33u, 0u);
    nevergone::hp_data::Reader empty_reader(empty_bytes);
    scene::FirstLayerObjectSequence empty;
    assert(scene::parse_first_layer_objects(empty_reader, &empty));
    assert(empty.objects.empty());
    assert(empty.bytes_consumed == 37u);

    // Declaring one more object than exists must fail transactionally.
    auto truncated_count = bytes;
    write_u32(truncated_count, 33u, 3u);
    nevergone::hp_data::Reader truncated_count_reader(truncated_count);
    scene::FirstLayerObjectSequence unchanged;
    unchanged.bytes_consumed = 999u;
    assert(!scene::parse_first_layer_objects(truncated_count_reader, &unchanged));
    assert(unchanged.bytes_consumed == 999u);

    // Hostile object counts are rejected before vector reserve.
    auto hostile_count = bytes;
    write_u32(hostile_count, 33u, 0xffffffffu);
    nevergone::hp_data::Reader hostile_count_reader(hostile_count);
    assert(!scene::parse_first_layer_objects(hostile_count_reader, &unchanged));
    assert(unchanged.bytes_consumed == 999u);

    // Exercise the generic top-level >2 version vector path independently.
    std::vector<std::uint8_t> versioned;
    append_core(versioned, 4, "v", 1u);
    append_u32(versioned, 1u); append_u32(versioned, 0xabcdef01u);
    append_u32(versioned, 101u); append_u32(versioned, 202u);
    append_u32(versioned, 1u); versioned.push_back(0x11u); versioned.push_back('x');
    append_i32(versioned, 0); append_i32(versioned, -44);
    nevergone::hp_data::Reader versioned_reader(versioned);
    scene::ObjectRecord versioned_record;
    assert(scene::parse_object_record_at(versioned_reader, 0u, 3, &versioned_record));
    assert(versioned_record.extra_u32_values.size() == 1u);
    assert(versioned_record.extra_u32_values[0] == 0xabcdef01u);
    assert(versioned_record.conditional_first_u32 == 101u);
    assert(versioned_record.conditional_second_u32 == 202u);
    assert(versioned_record.tail_i32 == -44);
    assert(versioned_record.end_offset == versioned.size());

    scene::ObjectRecord unchanged_record;
    unchanged_record.end_offset = 77u;
    assert(!scene::parse_object_record_at(reader, reader.size() + 1u, 2, &unchanged_record));
    assert(unchanged_record.end_offset == 77u);
    assert(!scene::parse_object_record_at(reader, 37u, 2, nullptr));
    assert(!scene::parse_first_layer_objects(reader, nullptr));
    return 0;
}
