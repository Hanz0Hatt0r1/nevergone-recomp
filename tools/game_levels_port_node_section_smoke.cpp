#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "game_levels_port_node_section.h"
#include "hp_data_reader.h"

namespace {

void append_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    assert(out != nullptr);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out->push_back(static_cast<std::uint8_t>(value >> shift));
    }
}
void append_string(std::vector<std::uint8_t>* out, const std::string& value, std::uint8_t gap) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out->push_back(gap);
    out->insert(out->end(), value.begin(), value.end());
}
void append_record(
        std::vector<std::uint8_t>* out,
        const std::string& first,
        const std::string& second,
        bool first_bool,
        bool second_bool,
        bool third_bool,
        const std::string& third) {
    append_string(out, first, 0xa1u);
    append_string(out, second, 0xb2u);
    append_u32(out, 11u);
    append_u32(out, 12u);
    out->push_back(first_bool ? 1u : 0u);
    out->push_back(second_bool ? 1u : 0u);
    out->push_back(third_bool ? 1u : 0u);
    append_u32(out, 13u);
    append_u32(out, 14u);
    append_u32(out, 15u);
    append_string(out, third, 0xc3u);
}

}  // namespace

int main() {
    namespace port = nevergone::game_levels_port_node_section;
    using nevergone::hp_data::Reader;

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 0u);
        Reader reader(bytes);
        port::Section section;
        assert(port::parse_section(reader, 0u, &section));
        assert(section.port_node_count == 0u);
        assert(section.port_nodes.empty());
        assert(section.end_offset == 4u);
    }

    {
        std::vector<std::uint8_t> bytes(5u, 0xeeu);
        const std::size_t start = bytes.size();
        append_u32(&bytes, 1u);
        append_record(&bytes, "a", "bc", true, false, true, "xyz");
        assert(bytes.size() == start + 48u);
        Reader reader(bytes);
        port::Section section;
        assert(port::parse_section(reader, start, &section));
        assert(section.start_offset == start);
        assert(section.end_offset == bytes.size());
        assert(section.port_nodes.size() == 1u);
        const auto& record = section.port_nodes[0];
        assert(record.first_string == "a");
        assert(record.second_string == "bc");
        assert(record.first_u32 == 11u);
        assert(record.second_u32 == 12u);
        assert(record.first_bool);
        assert(!record.second_bool);
        assert(record.third_bool);
        assert(record.third_u32 == 13u);
        assert(record.fourth_u32 == 14u);
        assert(record.fifth_u32 == 15u);
        assert(record.third_string == "xyz");
    }

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 2u);
        append_record(&bytes, "", "", false, false, false, "");
        append_record(&bytes, "q", "r", false, true, false, "s");
        Reader reader(bytes);
        port::Section section;
        assert(port::parse_section(reader, 0u, &section));
        assert(section.port_nodes.size() == 2u);
        assert(section.port_nodes[0].end_offset == section.port_nodes[1].start_offset);
        assert(section.end_offset == bytes.size());
    }

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 1u);
        append_record(&bytes, "truncated", "record", true, true, false, "tail");
        bytes.pop_back();
        Reader reader(bytes);
        port::Section unchanged;
        unchanged.port_node_count = 77u;
        unchanged.end_offset = 88u;
        assert(!port::parse_section(reader, 0u, &unchanged));
        assert(unchanged.port_node_count == 77u);
        assert(unchanged.end_offset == 88u);
    }

    {
        std::vector<std::uint8_t> bytes;
        append_u32(&bytes, 0xffffffffu);
        Reader reader(bytes);
        port::Section section;
        assert(!port::parse_section(reader, 0u, &section));
    }

    return 0;
}
