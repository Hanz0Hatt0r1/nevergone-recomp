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
// Later range construction now proves that the second value is the byte length
// of the immediately following string payload. Keep the historical field name
// here so existing probes remain source-compatible; FirstObjectCore exposes the
// newly verified string itself.
struct FirstObjectPrefix {
    FirstLayerHeader layer_header;
    std::int32_t first_i32 = 0;
    std::uint32_t second_u32 = 0;  // proven string byte length
    std::size_t bytes_consumed = 0;
};

// Strongest non-versioned first-object boundary. After the prefix the original
// advances by five bytes from the string-length field start (the four length
// bytes plus one still-opaque byte), copies exactly second_u32 bytes, appends a
// NUL, then reads five floats, one int32 and two one-byte bools.
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

bool parse(const hp_data::Reader& reader, Prefix* out);
bool parse_first_scene_header(const hp_data::Reader& reader, FirstSceneHeader* out);
bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out);
bool parse_first_object_prefix(const hp_data::Reader& reader, FirstObjectPrefix* out);

// Parse through the two proven bool fields and stop before the subsequent
// version-gated object block. Output is transactional.
bool parse_first_object_core(const hp_data::Reader& reader, FirstObjectCore* out);

// Parse the first proven version-gated object block. This deliberately stops
// before the subsequent branch on FirstObjectPrefix::first_i32. For top-level
// first_i32 <= 2, this succeeds without consuming bytes beyond FirstObjectCore.
bool parse_first_object_version_extension(
        const hp_data::Reader& reader,
        FirstObjectVersionExtension* out);

}  // namespace nevergone::game_levels_scene_prefix
