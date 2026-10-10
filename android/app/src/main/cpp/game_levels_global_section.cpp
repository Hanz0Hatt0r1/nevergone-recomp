#include "game_levels_global_section.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace nevergone::game_levels_global_section {
namespace {

constexpr std::size_t kMinimumStringRecordBytes = sizeof(std::uint32_t) + 1u;
constexpr std::size_t kIntIntFloatRecordBytes = sizeof(std::int32_t) * 2u + sizeof(float);
constexpr std::size_t kUintPairRecordBytes = sizeof(std::uint32_t) * 2u;
constexpr std::size_t kEnemyRecordBytes = sizeof(std::int32_t) * 7u + sizeof(float);

bool read_string_record(hp_data::Cursor* cursor, StringRecord* out) {
    if (cursor == nullptr || out == nullptr) return false;
    StringRecord parsed;
    if (!cursor->read_u32_le(&parsed.byte_length) || !cursor->skip(1u)) return false;
    const std::size_t length = static_cast<std::size_t>(parsed.byte_length);
    if (length > cursor->remaining() || !cursor->read_fixed_string(length, &parsed.value)) {
        return false;
    }
    *out = std::move(parsed);
    return true;
}

bool count_fits(std::uint32_t count, std::size_t remaining, std::size_t record_bytes) {
    return record_bytes != 0u && static_cast<std::size_t>(count) <= remaining / record_bytes;
}

}  // namespace

bool parse_section(
        const hp_data::Reader& reader,
        std::size_t start_offset,
        Section* out) {
    if (out == nullptr || start_offset > reader.size()) return false;

    hp_data::Cursor cursor(reader, start_offset);
    Section parsed;
    parsed.start_offset = start_offset;

    if (!cursor.read_u32_le(&parsed.string_count) ||
            !count_fits(parsed.string_count, cursor.remaining(), kMinimumStringRecordBytes)) {
        return false;
    }
    parsed.strings.reserve(static_cast<std::size_t>(parsed.string_count));
    for (std::uint32_t i = 0; i < parsed.string_count; ++i) {
        StringRecord record;
        if (!read_string_record(&cursor, &record)) return false;
        parsed.strings.push_back(std::move(record));
    }

    if (!cursor.read_u32_le(&parsed.first_u32) ||
            !cursor.read_u32_le(&parsed.second_u32) ||
            !cursor.read_u32_le(&parsed.int_int_float_count) ||
            !count_fits(
                    parsed.int_int_float_count,
                    cursor.remaining(),
                    kIntIntFloatRecordBytes)) {
        return false;
    }
    parsed.int_int_float_records.reserve(
            static_cast<std::size_t>(parsed.int_int_float_count));
    for (std::uint32_t i = 0; i < parsed.int_int_float_count; ++i) {
        IntIntFloatRecord record;
        if (!cursor.read_i32_le(&record.first_i32) ||
                !cursor.read_i32_le(&record.second_i32) ||
                !cursor.read_f32_le(&record.first_float)) {
            return false;
        }
        parsed.int_int_float_records.push_back(record);
    }

    if (!cursor.read_u32_le(&parsed.uint_pair_count) ||
            !count_fits(parsed.uint_pair_count, cursor.remaining(), kUintPairRecordBytes)) {
        return false;
    }
    parsed.uint_pair_records.reserve(static_cast<std::size_t>(parsed.uint_pair_count));
    for (std::uint32_t i = 0; i < parsed.uint_pair_count; ++i) {
        UintPairRecord record;
        if (!cursor.read_u32_le(&record.first_u32) ||
                !cursor.read_u32_le(&record.second_u32)) {
            return false;
        }
        parsed.uint_pair_records.push_back(record);
    }

    if (!cursor.read_u32_le(&parsed.enemy_count) ||
            !count_fits(parsed.enemy_count, cursor.remaining(), kEnemyRecordBytes)) {
        return false;
    }
    parsed.enemies.reserve(static_cast<std::size_t>(parsed.enemy_count));
    for (std::uint32_t i = 0; i < parsed.enemy_count; ++i) {
        EnemyRecord record;
        if (!cursor.read_i32_le(&record.first_i32) ||
                !cursor.read_i32_le(&record.second_i32) ||
                !cursor.read_f32_le(&record.first_float) ||
                !cursor.read_i32_le(&record.third_i32) ||
                !cursor.read_i32_le(&record.fourth_i32) ||
                !cursor.read_i32_le(&record.fifth_i32) ||
                !cursor.read_i32_le(&record.sixth_i32) ||
                !cursor.read_i32_le(&record.seventh_i32)) {
            return false;
        }
        parsed.enemies.push_back(record);
    }

    parsed.end_offset = cursor.offset();
    *out = std::move(parsed);
    return true;
}

}  // namespace nevergone::game_levels_global_section
