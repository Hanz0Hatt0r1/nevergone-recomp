#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "game_levels_global_section.h"
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
bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

}  // namespace

int main() {
    namespace global = nevergone::game_levels_global_section;
    using nevergone::hp_data::Reader;

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 0u);
        append_u32(&bytes, 11u);
        append_u32(&bytes, 12u);
        append_u32(&bytes, 0u);
        append_u32(&bytes, 0u);
        append_u32(&bytes, 0u);
        Reader reader(bytes);
        global::Section section;
        assert(global::parse_section(reader, 0u, &section));
        assert(section.end_offset == 24u);
        assert(section.strings.empty());
        assert(section.first_u32 == 11u);
        assert(section.second_u32 == 12u);
        assert(section.int_int_float_records.empty());
        assert(section.uint_pair_records.empty());
        assert(section.enemies.empty());
    }

    {
        std::vector<std::uint8_t> bytes(3u, 0xeeu);
        const std::size_t start = bytes.size();
        append_u32(&bytes, 2u);
        append_string(&bytes, "abc", 0xa1u);
        append_string(&bytes, "", 0xb2u);
        append_u32(&bytes, 21u);
        append_u32(&bytes, 22u);

        append_u32(&bytes, 2u);
        append_i32(&bytes, -1);
        append_i32(&bytes, 2);
        append_f32(&bytes, 3.5f);
        append_i32(&bytes, 4);
        append_i32(&bytes, -5);
        append_f32(&bytes, -6.25f);

        append_u32(&bytes, 1u);
        append_u32(&bytes, 31u);
        append_u32(&bytes, 32u);

        append_u32(&bytes, 1u);
        append_i32(&bytes, 41);
        append_i32(&bytes, 42);
        append_f32(&bytes, 43.5f);
        append_i32(&bytes, 44);
        append_i32(&bytes, 45);
        append_i32(&bytes, 46);
        append_i32(&bytes, 47);
        append_i32(&bytes, 48);
        assert(bytes.size() == start + 101u);

        Reader reader(bytes);
        global::Section section;
        assert(global::parse_section(reader, start, &section));
        assert(section.start_offset == start);
        assert(section.end_offset == bytes.size());
        assert(section.string_count == 2u);
        assert(section.strings[0].value == "abc");
        assert(section.strings[1].value.empty());
        assert(section.first_u32 == 21u);
        assert(section.second_u32 == 22u);
        assert(section.int_int_float_count == 2u);
        assert(section.int_int_float_records[0].first_i32 == -1);
        assert(section.int_int_float_records[0].second_i32 == 2);
        assert(near(section.int_int_float_records[0].first_float, 3.5f));
        assert(section.uint_pair_count == 1u);
        assert(section.uint_pair_records[0].first_u32 == 31u);
        assert(section.uint_pair_records[0].second_u32 == 32u);
        assert(section.enemy_count == 1u);
        assert(section.enemies[0].first_i32 == 41);
        assert(section.enemies[0].second_i32 == 42);
        assert(near(section.enemies[0].first_float, 43.5f));
        assert(section.enemies[0].third_i32 == 44);
        assert(section.enemies[0].seventh_i32 == 48);
    }

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 0u);
        append_u32(&bytes, 1u);
        append_u32(&bytes, 2u);
        append_u32(&bytes, 0u);
        append_u32(&bytes, 0u);
        append_u32(&bytes, 1u);
        append_i32(&bytes, 1);
        append_i32(&bytes, 2);
        append_f32(&bytes, 3.0f);
        append_i32(&bytes, 4);
        append_i32(&bytes, 5);
        append_i32(&bytes, 6);
        append_i32(&bytes, 7);
        append_i32(&bytes, 8);
        bytes.pop_back();
        Reader reader(bytes);
        global::Section unchanged;
        unchanged.string_count = 77u;
        unchanged.end_offset = 88u;
        assert(!global::parse_section(reader, 0u, &unchanged));
        assert(unchanged.string_count == 77u);
        assert(unchanged.end_offset == 88u);
    }

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 0xffffffffu);
        Reader reader(bytes);
        global::Section section;
        assert(!global::parse_section(reader, 0u, &section));
    }

    return 0;
}
