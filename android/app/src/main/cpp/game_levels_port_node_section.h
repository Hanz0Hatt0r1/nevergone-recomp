#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::game_levels_port_node_section {

struct PortNodeRecord {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::uint32_t first_string_length = 0;
    std::string first_string;
    std::uint32_t second_string_length = 0;
    std::string second_string;
    std::uint32_t first_u32 = 0;
    std::uint32_t second_u32 = 0;
    bool first_bool = false;
    bool second_bool = false;
    bool third_bool = false;
    std::uint32_t third_u32 = 0;
    std::uint32_t fourth_u32 = 0;
    std::uint32_t fifth_u32 = 0;
    std::uint32_t third_string_length = 0;
    std::string third_string;
};

struct Section {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::uint32_t port_node_count = 0;
    std::vector<PortNodeRecord> port_nodes;
};

bool parse_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        PortNodeRecord* out);

// Parses the complete evidence-backed LoadGL_PortNode section. end_offset is
// the exact stream position at which the original calls LinkScenePortNode().
bool parse_section(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        Section* out);

}  // namespace nevergone::game_levels_port_node_section
