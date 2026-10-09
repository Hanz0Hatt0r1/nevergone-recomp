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
std::vector<std::uint8_t> make_fixture() {
    std::vector<std::uint8_t> out;
    append_i32(out, 2); append_u32(out, 2u);
    append_u32(out, 3u); out.push_back(0x11u); out.insert(out.end(), {'o','n','e'});
    append_f32(out, 1.0f); append_f32(out, 2.0f); append_u32(out, 1u);
    append_f32(out, 0.5f); append_u32(out, 0u);
    append_u32(out, 0u); append_u32(out, 0u);
    assert(out.size() == 44u);
    append_u32(out, 3u); out.push_back(0x22u); out.insert(out.end(), {'t','w','o'});
    append_f32(out, 3.0f); append_f32(out, 4.0f); append_u32(out, 0u);
    assert(out.size() == 64u);
    return out;
}
}  // namespace

int main() {
    namespace scene = nevergone::game_levels_layer_tail;

    const auto bytes = make_fixture();
    nevergone::hp_data::Reader reader(bytes);

    scene::SceneRecord first;
    assert(scene::parse_scene_record_at(reader, 8u, 2, &first));
    assert(first.start_offset == 8u);
    assert(first.end_offset == 44u);
    assert(first.string_value == "one");
    assert(std::fabs(first.first_point_x - 1.0f) < 0.000001f);
    assert(first.layer_count == 1u);
    assert(first.layers.size() == 1u);
    assert(first.layers[0].start_offset == 28u);
    assert(first.layers[0].end_offset == 44u);

    scene::SceneRecord second;
    assert(scene::parse_scene_record_at(reader, first.end_offset, 2, &second));
    assert(second.start_offset == 44u);
    assert(second.end_offset == 64u);
    assert(second.string_value == "two");
    assert(std::fabs(second.first_point_y - 4.0f) < 0.000001f);
    assert(second.layer_count == 0u);
    assert(second.layers.empty());

    scene::SceneSection section;
    assert(scene::parse_scene_section(reader, &section));
    assert(section.prefix.first_i32 == 2);
    assert(section.prefix.scene_count == 2u);
    assert(section.scenes.size() == 2u);
    assert(section.scenes[0].end_offset == section.scenes[1].start_offset);
    assert(section.bytes_consumed == 64u);

    std::vector<std::uint8_t> zero_scenes;
    append_i32(zero_scenes, 7); append_u32(zero_scenes, 0u);
    nevergone::hp_data::Reader zero_reader(zero_scenes);
    scene::SceneSection zero;
    assert(scene::parse_scene_section(zero_reader, &zero));
    assert(zero.scenes.empty());
    assert(zero.bytes_consumed == 8u);

    auto hostile_count = bytes;
    write_u32(hostile_count, 4u, 0xffffffffu);
    nevergone::hp_data::Reader hostile_count_reader(hostile_count);
    scene::SceneSection unchanged;
    unchanged.bytes_consumed = 777u;
    assert(!scene::parse_scene_section(hostile_count_reader, &unchanged));
    assert(unchanged.bytes_consumed == 777u);

    auto truncated = bytes;
    truncated.pop_back();
    nevergone::hp_data::Reader truncated_reader(truncated);
    assert(!scene::parse_scene_section(truncated_reader, &unchanged));
    assert(unchanged.bytes_consumed == 777u);

    auto hostile_layers = bytes;
    write_u32(hostile_layers, 60u, 0xffffffffu);
    nevergone::hp_data::Reader hostile_layers_reader(hostile_layers);
    scene::SceneRecord unchanged_scene;
    unchanged_scene.end_offset = 555u;
    assert(!scene::parse_scene_record_at(hostile_layers_reader, 44u, 2, &unchanged_scene));
    assert(unchanged_scene.end_offset == 555u);

    assert(!scene::parse_scene_record_at(reader, reader.size() + 1u, 2, &unchanged_scene));
    assert(unchanged_scene.end_offset == 555u);
    assert(!scene::parse_scene_record_at(reader, 8u, 2, nullptr));
    assert(!scene::parse_scene_section(reader, nullptr));
    return 0;
}
