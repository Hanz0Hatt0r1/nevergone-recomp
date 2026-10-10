#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::enemy_actions_wbg_prefix {

// The original ARMv7 loadWBGFile() consumes these four fields before entering
// its first ActionFrameData loop.
constexpr std::size_t kPrefixBytes = 16u;

// One ActionFrameData stream record has 76 evidence-backed non-payload bytes:
// int32 + 12 floats + bool8 + int32 + float, then three int32 string lengths,
// each followed by one skipped byte. String payload bytes are additional.
constexpr std::size_t kActionFrameFixedBytes = 76u;
constexpr std::size_t kStringSeparatorBytes = 1u;

// The counted block immediately after the first ActionFrameData loop stores one
// int32 followed by two float32 values per record.
constexpr std::size_t kSecondaryRecordBytes = 12u;

// Each original temporary char buffer occupies 0x100 bytes and receives an
// explicit NUL at buffer[length]. The reconstructed parser rejects payloads
// that would cross that observed stack-buffer boundary.
constexpr std::size_t kMaxStringPayloadBytes = 0xffu;

struct Prefix {
    std::int32_t first_i32 = 0;
    float first_float = 0.0f;
    float second_float = 0.0f;
    std::uint32_t action_frame_count = 0;
    std::size_t bytes_consumed = 0;
};

struct ActionFrameRecord {
    std::int32_t first_i32 = 0;
    std::array<float, 12> float_values{};
    bool first_bool = false;
    std::int32_t second_i32 = 0;
    float trailing_float = 0.0f;

    // The original reads these lengths through the signed-int HPData overload
    // and then uses each value as the following char-copy byte count.
    std::int32_t first_string_length_i32 = 0;
    std::string first_string;
    std::int32_t second_string_length_i32 = 0;
    std::string second_string;
    std::int32_t third_string_length_i32 = 0;
    std::string third_string;

    std::size_t bytes_consumed = 0;
};

struct SecondaryRecord {
    std::int32_t first_i32 = 0;
    float first_float = 0.0f;
    float second_float = 0.0f;
    std::size_t bytes_consumed = 0;
};

// Bounded clean-room representation of the stream through the end of the
// counted block beginning at original instruction 0x28f842. The signed count
// is retained exactly: native control flow performs zero iterations for values
// <= 0 rather than reinterpreting them as unsigned.
struct InitialSections {
    Prefix prefix;
    std::vector<ActionFrameRecord> action_frames;
    std::int32_t secondary_record_count_i32 = 0;
    std::vector<SecondaryRecord> secondary_records;
    std::size_t bytes_consumed = 0;
};

// Parsers are transactional: *out is changed only after the complete requested
// evidence-backed boundary has been consumed successfully.
bool parse_prefix(const hp_data::Reader& reader, Prefix* out);
bool parse_action_frame_record(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        ActionFrameRecord* out);
bool parse_secondary_record(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        SecondaryRecord* out);
bool parse_initial_sections(const hp_data::Reader& reader, InitialSections* out);

}  // namespace nevergone::enemy_actions_wbg_prefix
