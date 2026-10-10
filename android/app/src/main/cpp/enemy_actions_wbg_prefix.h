#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::enemy_actions_wbg_prefix {

constexpr std::size_t kPrefixBytes = 16u;
constexpr std::size_t kActionFrameFixedBytes = 76u;
constexpr std::size_t kStringSeparatorBytes = 1u;

constexpr std::size_t kCompactBlockHeaderBytes = 4u;
constexpr std::size_t kCompactActionFrameBytes = 12u;

constexpr std::size_t kNestedBlockHeaderBytes = 4u;
constexpr std::size_t kNestedGroupHeaderBytes = 4u;
constexpr std::size_t kNestedActionFrameFixedBytes = 72u;

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

// Section E starts with a signed int32 count. Every positive iteration consumes
// exactly int32 + 12 float32 + bool8 = 0x35 bytes before creating an AFD and
// appending it to EnemyActionsData+0x84.
constexpr std::size_t kFixedTailBlockHeaderBytes = 4u;
constexpr std::size_t kFixedTailActionFrameBytes = 0x35u;

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

struct FixedTailActionFrameRecord {
    std::int32_t first_i32 = 0;
    std::array<float, 12> float_values{};
    bool first_bool = false;
};

struct FixedTailActionFrameBlock {
    // Native loop uses signed BGE: zero and negative values consume no records.
    std::int32_t count_i32 = 0;
    std::vector<FixedTailActionFrameRecord> records;
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
bool parse_fixed_tail_action_frame_block(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        FixedTailActionFrameBlock* out);

}  // namespace nevergone::enemy_actions_wbg_prefix
