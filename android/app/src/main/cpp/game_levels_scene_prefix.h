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

// Generic evidence-backed object record independent of scene/layer position.
// start_offset/end_offset are absolute reader offsets. Field names remain
// intentionally structural where gameplay semantics are not yet proven.
struct ObjectRecord {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::int32_t first_i32 = 0;
    std::uint32_t string_length = 0;
    std::string string_value;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    float middle_float = 0.0f;
    float second_point_x = 0.0f;
    float second_point_y = 0.0f;
    std::int32_t trailing_i32 = 0;
    bool first_bool = false;
    bool second_bool = false;
    std::vector<std::uint32_t> extra_u32_values;
    bool conditional_present = false;
    std::uint32_t conditional_first_u32 = 0;
    std::uint32_t conditional_second_u32 = 0;
    std::uint32_t conditional_string_length = 0;
    std::string conditional_string_value;
    std::int32_t primary_i32 = 0;
    std::int32_t secondary_i32 = 0;
    bool has_tail_i32 = false;
    std::int32_t tail_i32 = 0;
    bool has_tail_points = false;
    float first_tail_point_x = 0.0f;
    float first_tail_point_y = 0.0f;
    float second_tail_point_x = 0.0f;
    float second_tail_point_y = 0.0f;
};

struct FirstLayerObjectSequence {
    FirstLayerHeader layer_header;
    std::vector<ObjectRecord> objects;
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
bool parse_first_object_record(const hp_data::Reader& reader, FirstObjectRecord* out);

// Parse one complete object record at an explicit stream offset using the
// already recovered top-level signed format gate. The returned end_offset is
// the exact next sequential stream position after the common AddObject join.
bool parse_object_record_at(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t top_level_gate,
        ObjectRecord* out);

// Parse every object in the first layer by chaining each complete record's
// end_offset. This mirrors the original object-loop stream progression and
// stops before post-object layer data.
bool parse_first_layer_objects(
        const hp_data::Reader& reader,
        FirstLayerObjectSequence* out);

}  // namespace nevergone::game_levels_scene_prefix
