#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game_levels_global_section.h"

namespace nevergone::game_levels_enemy_record_mapping {

// Only the seven integer slots from the recovered 32-byte EnemyRecord can be
// selected here. The intervening float remains intentionally unavailable for
// identity/base-health semantics.
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

// A cross-evidence candidate is deliberately not usable by production. The
// identity and base-health fields must also be distinct.
bool is_usable(const Mapping& mapping);

bool resolve_base_health(
        const std::vector<game_levels_global_section::EnemyRecord>& records,
        const Mapping& mapping,
        unsigned enemy_id,
        int* out_base_health);

// Signature-compatible adapter for tutorial_enemy_roster::BaseHealthResolver.
bool roster_base_health_resolver(
        unsigned enemy_id,
        int* out_base_health,
        void* user_data);

}  // namespace nevergone::game_levels_enemy_record_mapping
