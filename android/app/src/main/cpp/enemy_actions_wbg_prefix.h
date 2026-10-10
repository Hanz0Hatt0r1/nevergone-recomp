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

// One primary ActionFrameData stream record has 76 evidence-backed non-payload
// bytes: int32 + 12 floats + bool8 + int32 + float, then three int32 string
// lengths, each followed by one skipped byte. String payload bytes are extra.
constexpr std::size_t kActionFrameFixedBytes = 76u;
constexpr std::size_t kStringSeparatorBytes = 1u;

// The next counted block starts with one signed int32 count and each positive
// iteration consumes exactly int32 + float + float before creating an AFD.
constexpr std::size_t kCompactBlockHeaderBytes = 4u;
constexpr std::size_t kCompactActionFrameBytes = 12u;

// Section C starts with a signed outer count. Every positive outer iteration
// starts with an unsigned inner count. Each inner record has 72 fixed bytes:
// int32 + 12 floats + bool8 + int32 + three framed string lengths.
constexpr std::size_t kNestedBlockHeaderBytes = 4u;
constexpr std::size_t kNestedGroupHeaderBytes = 4u;
constexpr std::size_t kNestedActionFrameFixedBytes = 72u;

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

struct CompactActionFrameRecord {
    std::int32_t first_i32 = 0;
    float first_float = 0.0f;
    float second_float = 0.0f;
};

struct CompactActionFrameBlock {
    // Native control flow uses a signed BGE test. Zero and negative values
    // therefore consume no records and proceed to the following block.
    std::int32_t count_i32 = 0;
    std::vector<CompactActionFrameRecord> records;
    std::size_t bytes_consumed = 0;
};

struct NestedActionFrameRecord {
    std::int32_t first_i32 = 0;
    std::array<float, 12> float_values{};
    bool first_bool = false;
    std::int32_t second_i32 = 0;
    std::int32_t first_string_length_i32 = 0;
    std::string first_string;
    std::int32_t second_string_length_i32 = 0;
    std::string second_string;
    std::int32_t third_string_length_i32 = 0;
    std::string third_string;
    std::size_t bytes_consumed = 0;
};

struct NestedActionFrameGroup {
    std::uint32_t record_count = 0;
    std::vector<NestedActionFrameRecord> records;
    std::size_t bytes_consumed = 0;
};

struct NestedActionFrameBlock {
    // The original outer loop uses a signed BGE comparison. A nonpositive
    // value consumes only this four-byte field. Positive groups each use one
    // unsigned inner count and target EnemyActionsData + 0x14 + 4*group_index.
    std::int32_t group_count_i32 = 0;
    std::vector<NestedActionFrameGroup> groups;
    std::size_t bytes_consumed = 0;
};

// Parsers are transactional: *out is changed only after the complete
// evidence-backed boundary has been consumed successfully.
bool parse_prefix(const hp_data::Reader& reader, Prefix* out);
bool parse_action_frame_record(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        ActionFrameRecord* out);
bool parse_compact_action_frame_block(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        CompactActionFrameBlock* out);
bool parse_nested_action_frame_block(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        NestedActionFrameBlock* out);

}  // namespace nevergone::enemy_actions_wbg_prefix
