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

// ARMv7 proves the first object begins with an int32 followed by a uint32.
// Later range construction proves that the second value is the byte length of
// the immediately following string payload. Keep the historical field name so
// existing probes remain source-compatible.
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

// Immediately after FirstObjectCore, original LoadGL_Scene tests the top-level
// signed first_i32 against 2. Values <= 2 consume no bytes. Values > 2 read a
// uint32 count followed by exactly that many uint32 values into an object-owned
// vector. The semantic meaning of the values is not yet recovered.
struct FirstObjectVersionExtension {
    FirstObjectCore core;
    std::vector<std::uint32_t> extra_u32_values;
    std::size_t bytes_consumed = 0;
};

// The next branch tests the object's leading first_i32. A zero value jumps
// directly to AddObject and consumes no further bytes. For nonzero values the
// original reads one uint32 when the top-level gate <= 1, otherwise two; then a
// uint32 string length, one skipped byte, exactly that many chars, an int32 and
// a second int32 only when the first int32 equals 1. Semantics stay opaque.
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

bool parse(const hp_data::Reader& reader, Prefix* out);
bool parse_first_scene_header(const hp_data::Reader& reader, FirstSceneHeader* out);
bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out);
bool parse_first_object_prefix(const hp_data::Reader& reader, FirstObjectPrefix* out);
bool parse_first_object_core(const hp_data::Reader& reader, FirstObjectCore* out);
bool parse_first_object_version_extension(
        const hp_data::Reader& reader,
        FirstObjectVersionExtension* out);

// Parse through the shared conditional object header and stop before the
// following object-type-specific branch (the branch beginning with the
// recovered first_i32 values 4/6/9/10). Output is transactional.
bool parse_first_object_conditional_header(
        const hp_data::Reader& reader,
        FirstObjectConditionalHeader* out);

}  // namespace nevergone::game_levels_scene_prefix
