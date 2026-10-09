#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "game_levels_actions_section.h"
#include "hp_data_reader.h"

namespace {

void append_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    assert(out != nullptr);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out->push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void append_i32(std::vector<std::uint8_t>* out, std::int32_t value) {
    append_u32(out, static_cast<std::uint32_t>(value));
}

void append_f32(std::vector<std::uint8_t>* out, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(out, bits);
}

void append_string(std::vector<std::uint8_t>* out, const std::string& value, std::uint8_t gap) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out->push_back(gap);
    out->insert(out->end(), value.begin(), value.end());
}

void append_record(
        std::vector<std::uint8_t>* out,
        std::int32_t gate,
        const std::string& first,
        const std::string& second,
        std::uint32_t first_u32,
        std::uint32_t second_u32,
        std::int32_t first_i32,
        float x,
        float y) {
    append_string(out, first, 0xa1u);
    append_string(out, second, 0xb2u);
    append_u32(out, first_u32);
    append_u32(out, second_u32);
    append_i32(out, first_i32);
    append_f32(out, x);
    append_f32(out, y);
    if (gate > 1) {
        append_i32(out, 101);
        append_i32(out, 102);
    }
    if (gate > 3) {
        append_i32(out, 103);
        append_i32(out, 104);
    }
}

bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

}  // namespace

int main() {
    namespace actions = nevergone::game_levels_actions_section;
    using nevergone::hp_data::Reader;

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 0u);
        Reader reader(bytes);
        actions::Section section;
        assert(actions::parse_section(reader, 0u, 4, &section));
        assert(section.action_count == 0u);
        assert(section.actions.empty());
        assert(section.end_offset == 4u);
    }

    {
        constexpr std::int32_t kGate = 4;
        std::vector<std::uint8_t> bytes(7u, 0xeeu);
        append_u32(&bytes, 2u);
        const std::size_t section_start = 7u;
        append_record(&bytes, kGate, "a", "bc", 11u, 12u, -13, 1.5f, -2.5f);
        append_record(&bytes, kGate, "", "", 21u, 22u, 23, 3.25f, 4.5f);
        assert(bytes.size() == section_start + 99u);

        Reader reader(bytes);
        actions::Section section;
        assert(actions::parse_section(reader, section_start, kGate, &section));
        assert(section.start_offset == section_start);
        assert(section.end_offset == bytes.size());
        assert(section.actions.size() == 2u);

        const auto& first = section.actions[0];
        assert(first.first_string == "a");
        assert(first.second_string == "bc");
        assert(first.first_u32 == 11u);
        assert(first.second_u32 == 12u);
        assert(first.first_i32 == -13);
        assert(near(first.point_x, 1.5f));
        assert(near(first.point_y, -2.5f));
        assert(first.has_gate_gt1_fields);
        assert(first.gate_gt1_first_i32 == 101);
        assert(first.gate_gt1_second_i32 == 102);
        assert(first.has_gate_gt3_fields);
        assert(first.gate_gt3_first_i32 == 103);
        assert(first.gate_gt3_second_i32 == 104);
        assert(first.end_offset == section.actions[1].start_offset);
    }

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 1u);
        append_record(&bytes, 1, "x", "y", 1u, 2u, 3, 4.0f, 5.0f);
        Reader reader(bytes);
        actions::Section section;
        assert(actions::parse_section(reader, 0u, 1, &section));
        assert(section.actions.size() == 1u);
        assert(!section.actions[0].has_gate_gt1_fields);
        assert(!section.actions[0].has_gate_gt3_fields);
        assert(section.end_offset == bytes.size());
    }

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 1u);
        append_record(&bytes, 2, "left", "right", 1u, 2u, 3, 4.0f, 5.0f);
        Reader reader(bytes);
        actions::Section section;
        assert(actions::parse_section(reader, 0u, 2, &section));
        assert(section.actions[0].has_gate_gt1_fields);
        assert(!section.actions[0].has_gate_gt3_fields);
    }

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 1u);
        append_record(&bytes, 4, "truncated", "record", 1u, 2u, 3, 4.0f, 5.0f);
        bytes.pop_back();
        Reader reader(bytes);
        actions::Section unchanged;
        unchanged.action_count = 77u;
        unchanged.end_offset = 88u;
        assert(!actions::parse_section(reader, 0u, 4, &unchanged));
        assert(unchanged.action_count == 77u);
        assert(unchanged.end_offset == 88u);
    }

    {
        std::vector<std::uint8_t> hostile;
        append_u32(&hostile, 0xffffffffu);
        Reader reader(hostile);
        actions::Section section;
        assert(!actions::parse_section(reader, 0u, 0, &section));
    }

    return 0;
}
