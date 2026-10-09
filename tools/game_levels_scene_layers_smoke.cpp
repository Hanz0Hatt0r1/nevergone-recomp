#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "game_levels_layer_tail.h"
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
    for (int i = 0; i < 5; ++i) append_f32(out, static_cast<float>(i + 1));
    append_i32(out, 6); out.push_back(1u); out.push_back(0u);
}
std::vector<std::uint8_t> make_fixture() {
    std::vector<std::uint8_t> out;
    append_i32(out, 2); append_u32(out, 1u);
    append_u32(out, 4u); out.push_back(0x7fu); out.insert(out.end(), {'h','e','r','o'});
    append_f32(out, 1.0f); append_f32(out, 2.0f); append_u32(out, 2u);
    assert(out.size() == 29u);

    append_f32(out, 0.5f); append_u32(out, 1u);
    append_core(out, 0, "a", 1u);
    append_u32(out, 1u); append_f32(out, 10.0f); append_f32(out, 20.0f);
    append_u32(out, 0u);
    assert(out.size() == 89u);

    append_f32(out, 1.5f); append_u32(out, 0u);
    append_u32(out, 0u);
    append_u32(out, 1u); append_f32(out, 30.0f); append_f32(out, 40.0f);
    assert(out.size() == 113u);
    return out;
}
}  // namespace

int main() {
    namespace layers = nevergone::game_levels_layer_tail;

    const auto bytes = make_fixture();
    nevergone::hp_data::Reader reader(bytes);

    layers::LayerRecord first;
    assert(layers::parse_layer_record_at(reader, 29u, 2, &first));
    assert(first.start_offset == 29u);
    assert(first.end_offset == 89u);
    assert(std::fabs(first.first_float - 0.5f) < 0.000001f);
    assert(first.object_count == 1u);
    assert(first.objects.size() == 1u);
    assert(first.objects[0].string_value == "a");
    assert(first.top_border_points.size() == 1u);
    assert(first.bottom_border_points.empty());
    assert(std::fabs(first.top_border_points[0].y - 20.0f) < 0.000001f);

    layers::LayerRecord second;
    assert(layers::parse_layer_record_at(reader, first.end_offset, 2, &second));
    assert(second.start_offset == 89u);
    assert(second.end_offset == 113u);
    assert(std::fabs(second.first_float - 1.5f) < 0.000001f);
    assert(second.object_count == 0u);
    assert(second.objects.empty());
    assert(second.top_border_points.empty());
    assert(second.bottom_border_points.size() == 1u);
    assert(std::fabs(second.bottom_border_points[0].x - 30.0f) < 0.000001f);

    layers::FirstSceneLayerSequence sequence;
    assert(layers::parse_first_scene_layers(reader, &sequence));
    assert(sequence.scene_header.layer_count == 2u);
    assert(sequence.layers.size() == 2u);
    assert(sequence.layers[0].end_offset == sequence.layers[1].start_offset);
    assert(sequence.bytes_consumed == 113u);

    // A zero layer count completes at the scene-header boundary.
    auto zero_layers = bytes;
    write_u32(zero_layers, 25u, 0u);
    nevergone::hp_data::Reader zero_reader(zero_layers);
    layers::FirstSceneLayerSequence empty;
    assert(layers::parse_first_scene_layers(zero_reader, &empty));
    assert(empty.layers.empty());
    assert(empty.bytes_consumed == 29u);

    // A hostile layer count is rejected before vector reserve.
    auto hostile = bytes;
    write_u32(hostile, 25u, 0xffffffffu);
    nevergone::hp_data::Reader hostile_reader(hostile);
    layers::FirstSceneLayerSequence unchanged;
    unchanged.bytes_consumed = 777u;
    assert(!layers::parse_first_scene_layers(hostile_reader, &unchanged));
    assert(unchanged.bytes_consumed == 777u);

    // Declaring the second layer but truncating its bottom point is transactional.
    auto truncated = bytes;
    truncated.pop_back();
    nevergone::hp_data::Reader truncated_reader(truncated);
    assert(!layers::parse_first_scene_layers(truncated_reader, &unchanged));
    assert(unchanged.bytes_consumed == 777u);

    // Impossible object count inside a generic layer is rejected before reserve.
    auto hostile_objects = bytes;
    write_u32(hostile_objects, 33u, 0xffffffffu);
    nevergone::hp_data::Reader hostile_objects_reader(hostile_objects);
    layers::LayerRecord unchanged_layer;
    unchanged_layer.end_offset = 555u;
    assert(!layers::parse_layer_record_at(hostile_objects_reader, 29u, 2, &unchanged_layer));
    assert(unchanged_layer.end_offset == 555u);

    assert(!layers::parse_layer_record_at(reader, reader.size() + 1u, 2, &unchanged_layer));
    assert(unchanged_layer.end_offset == 555u);
    assert(!layers::parse_layer_record_at(reader, 29u, 2, nullptr));
    assert(!layers::parse_first_scene_layers(reader, nullptr));
    return 0;
}
