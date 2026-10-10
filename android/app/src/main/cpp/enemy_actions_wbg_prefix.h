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

// Section D has no serialized outer count. The first WBG header word selects
// exactly 6 or 20 groups. Each group starts with one signed record count and
// each record has 80 fixed bytes before its three variable payloads.
constexpr std::int32_t kVersionedGroupGateValue = 0x68;
constexpr std::size_t kLegacyVersionedGroupCount = 6u;
constexpr std::size_t kModernVersionedGroupCount = 20u;
constexpr std::size_t kVersionedGroupHeaderBytes = 4u;
constexpr std::size_t kVersionedActionFrameFixedBytes = 80u;

inline std::size_t versioned_group_count_for_header(std::int32_t header_word0) {
    return header_word0 > kVersionedGroupGateValue
            ? kModernVersionedGroupCount
            : kLegacyVersionedGroupCount;
}

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
    std::int32_t group_count_i32 = 0;
    std::vector<NestedActionFrameGroup> groups;
    std::size_t bytes_consumed = 0;
};

struct VersionedActionFrameRecord {
    std::int32_t first_i32 = 0;
    std::array<float, 12> float_values{};
    bool first_bool = false;
    std::int32_t second_i32 = 0;
    std::uint32_t first_u32 = 0;
    std::uint32_t second_u32 = 0;
    std::int32_t first_string_length_i32 = 0;
    std::string first_string;
    std::int32_t second_string_length_i32 = 0;
    std::string second_string;
    std::int32_t third_string_length_i32 = 0;
    std::string third_string;
    std::size_t bytes_consumed = 0;
};

struct VersionedActionFrameGroup {
    // Native inner guard uses signed BGE, so nonpositive counts consume no
    // records but the group header is still present for all 6/20 groups.
    std::int32_t record_count_i32 = 0;
    std::vector<VersionedActionFrameRecord> records;
    std::size_t bytes_consumed = 0;
};

struct VersionedActionFrameBlock {
    std::int32_t header_word0 = 0;
    std::size_t expected_group_count = 0;
    std::vector<VersionedActionFrameGroup> groups;
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
bool parse_versioned_action_frame_groups(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        std::int32_t header_word0,
        VersionedActionFrameBlock* out);

}  // namespace nevergone::enemy_actions_wbg_prefix
