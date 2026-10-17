#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "game_levels_global_section.h"

namespace nevergone::game_levels_enemy_record_mapping {

// Only the seven integer slots from the recovered 32-byte EnemyRecord can be
// selected as identity/base-health fields. The intervening float has its own
// reviewed combat consumer and is not selectable as an integer field.
enum class IntegerField {
    kFirstI32 = 0,
    kSecondI32,
    kThirdI32,
    kFourthI32,
    kFifthI32,
    kSixthI32,
    kSeventhI32,
};

enum class ReviewStatus {
    kCandidate = 0,
    kReviewed,
};

struct Mapping {
    IntegerField identity_field = IntegerField::kFirstI32;
    IntegerField base_health_field = IntegerField::kFirstI32;
    ReviewStatus review_status = ReviewStatus::kCandidate;
};

struct ResolverContext {
    const Mapping* mapping = nullptr;
    const std::vector<game_levels_global_section::EnemyRecord>* records = nullptr;
};

const char* field_name(IntegerField field);
bool parse_field_name(const std::string& name, IntegerField* out);

// Exact original ARMv7 evidence closes both semantic fields. LoadGL_Global
// stores first_i32 at GameLevelsEnemyData +0x14 and second_i32 at +0x18/+0x1c;
// GetEnemyDataWithEnemyID consumes +0x14, while EnemyObject::loadEnemyObjectFile
// seeds health from +0x18. This helper is therefore reviewed production
// evidence, not a promoted heuristic candidate.
Mapping reviewed_original_mapping();

// A cross-evidence candidate is deliberately not usable by production. The
// identity and base-health fields must also be distinct.
bool is_usable(const Mapping& mapping);

bool resolve_base_health(
        const std::vector<game_levels_global_section::EnemyRecord>& records,
        const Mapping& mapping,
        unsigned enemy_id,
        int* out_base_health);

// Resolves the separately recovered EnemyRecord.first_float combat input for
// exactly one record selected by the reviewed identity mapping.
std::optional<std::uint32_t> resolve_input_value_2bc(
        const std::vector<game_levels_global_section::EnemyRecord>& records,
        const Mapping& mapping,
        unsigned enemy_id);

// Signature-compatible adapter for tutorial_enemy_roster::BaseHealthResolver.
bool roster_base_health_resolver(
        unsigned enemy_id,
        int* out_base_health,
        void* user_data);

}  // namespace nevergone::game_levels_enemy_record_mapping
