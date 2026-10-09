#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::game_levels_scene_prefix {

struct Prefix {
    std::int32_t first_i32 = 0;
    std::uint32_t scene_count = 0;
    std::size_t bytes_consumed = 0;
};

struct FirstSceneHeader {
    Prefix prefix;
    std::uint32_t first_string_length = 0;
    std::string first_string;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    std::uint32_t layer_count = 0;
    std::size_t bytes_consumed = 0;
};

struct FirstLayerHeader {
    FirstSceneHeader scene_header;
    float first_float = 0.0f;
    std::uint32_t object_count = 0;
    std::size_t bytes_consumed = 0;
};

struct FirstObjectPrefix {
    FirstLayerHeader layer_header;
    std::int32_t first_i32 = 0;
    std::uint32_t second_u32 = 0;  // proven string byte length
    std::size_t bytes_consumed = 0;
};

struct FirstObjectCore {
    FirstObjectPrefix prefix;
    std::string string_value;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    float middle_float = 0.0f;
    float second_point_x = 0.0f;
    float second_point_y = 0.0f;
    std::int32_t trailing_i32 = 0;
    bool first_bool = false;
    bool second_bool = false;
    std::size_t bytes_consumed = 0;
};

struct FirstObjectVersionExtension {
    FirstObjectCore core;
    std::vector<std::uint32_t> extra_u32_values;
    std::size_t bytes_consumed = 0;
};

struct FirstObjectConditionalHeader {
    FirstObjectVersionExtension extension;
    bool present = false;
    std::uint32_t first_u32 = 0;
    std::uint32_t second_u32 = 0;
    std::uint32_t string_length = 0;
    std::string string_value;
    std::int32_t primary_i32 = 0;
    std::int32_t secondary_i32 = 0;
    std::size_t bytes_consumed = 0;
};

// ARMv7 shows no further HPData reads between this type-specific tail and the
// common AddObject call. Types 4/6/9 read one int32. Type 10 reads one int32
// plus four floats that populate two CCPoints. All other values consume no tail
// bytes. Once this structure parses successfully, the first object record has a
// complete evidence-backed byte extent.
struct FirstObjectRecord {
    FirstObjectConditionalHeader header;
    bool has_tail_i32 = false;
    std::int32_t tail_i32 = 0;
    bool has_tail_points = false;
    float first_tail_point_x = 0.0f;
    float first_tail_point_y = 0.0f;
    float second_tail_point_x = 0.0f;
    float second_tail_point_y = 0.0f;
    std::size_t bytes_consumed = 0;
};

bool parse(const hp_data::Reader& reader, Prefix* out);
bool parse_first_scene_header(const hp_data::Reader& reader, FirstSceneHeader* out);
bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out);
bool parse_first_object_prefix(const hp_data::Reader& reader, FirstObjectPrefix* out);
bool parse_first_object_core(const hp_data::Reader& reader, FirstObjectCore* out);
bool parse_first_object_version_extension(
        const hp_data::Reader& reader,
        FirstObjectVersionExtension* out);
bool parse_first_object_conditional_header(
        const hp_data::Reader& reader,
        FirstObjectConditionalHeader* out);

// Parse the entire first object record through the final type-specific stream
// reads and the common AddObject join point. Output is transactional.
bool parse_first_object_record(const hp_data::Reader& reader, FirstObjectRecord* out);

}  // namespace nevergone::game_levels_scene_prefix
