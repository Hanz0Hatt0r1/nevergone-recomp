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
    append_f32(out, 1.0f); append_f32(out, 2.0f); append_u32(out, 1u);
    append_f32(out, 0.5f); append_u32(out, 1u);
    append_core(out, 0, "a", 1u);
    assert(out.size() == 73u);
    append_u32(out, 2u);
    append_f32(out, 10.0f); append_f32(out, 20.0f);
    append_f32(out, 30.0f); append_f32(out, 40.0f);
    append_u32(out, 1u);
    append_f32(out, 50.0f); append_f32(out, 60.0f);
    assert(out.size() == 105u);
    return out;
}
}  // namespace

int main() {
    namespace tail = nevergone::game_levels_layer_tail;

    const auto bytes = make_fixture();
    nevergone::hp_data::Reader reader(bytes);
    tail::FirstLayerRecord record;
    assert(tail::parse_first_layer_record(reader, &record));
    assert(record.object_sequence.objects.size() == 1u);
    assert(record.object_sequence.bytes_consumed == 73u);
    assert(record.top_border_points.size() == 2u);
    assert(record.bottom_border_points.size() == 1u);
    assert(std::fabs(record.top_border_points[0].x - 10.0f) < 0.000001f);
    assert(std::fabs(record.top_border_points[1].y - 40.0f) < 0.000001f);
    assert(std::fabs(record.bottom_border_points[0].x - 50.0f) < 0.000001f);
    assert(std::fabs(record.bottom_border_points[0].y - 60.0f) < 0.000001f);
    assert(record.bytes_consumed == 105u);

    // Both point lists may be empty; the two count fields still consume 8 bytes.
    std::vector<std::uint8_t> empty = bytes;
    empty.resize(73u);
    append_u32(empty, 0u);
    append_u32(empty, 0u);
    nevergone::hp_data::Reader empty_reader(empty);
    tail::FirstLayerRecord empty_record;
    assert(tail::parse_first_layer_record(empty_reader, &empty_record));
    assert(empty_record.top_border_points.empty());
    assert(empty_record.bottom_border_points.empty());
    assert(empty_record.bytes_consumed == 81u);

    // A hostile top count is rejected before reserve and must leave output unchanged.
    std::vector<std::uint8_t> hostile = bytes;
    hostile.resize(77u);
    write_u32(hostile, 73u, 0xffffffffu);
    nevergone::hp_data::Reader hostile_reader(hostile);
    tail::FirstLayerRecord unchanged;
    unchanged.bytes_consumed = 999u;
    assert(!tail::parse_first_layer_record(hostile_reader, &unchanged));
    assert(unchanged.bytes_consumed == 999u);

    // A valid top list followed by a truncated bottom list is also transactional.
    std::vector<std::uint8_t> truncated = bytes;
    truncated.resize(73u);
    append_u32(truncated, 1u);
    append_f32(truncated, 1.0f); append_f32(truncated, 2.0f);
    append_u32(truncated, 1u);
    append_f32(truncated, 3.0f);  // missing y
    nevergone::hp_data::Reader truncated_reader(truncated);
    assert(!tail::parse_first_layer_record(truncated_reader, &unchanged));
    assert(unchanged.bytes_consumed == 999u);

    assert(!tail::parse_first_layer_record(reader, nullptr));
    return 0;
}
