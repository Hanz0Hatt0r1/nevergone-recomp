#include "game_levels_enemy_record_mapping.h"

#include <cstdint>
#include <limits>

#include "enemy_attack_value_contract.h"

namespace nevergone::game_levels_enemy_record_mapping {
namespace {

bool read_field(
        const game_levels_global_section::EnemyRecord& record,
        IntegerField field,
        std::int32_t* out) {
    if (out == nullptr) return false;
    switch (field) {
        case IntegerField::kFirstI32:
            *out = record.first_i32;
            return true;
        case IntegerField::kSecondI32:
            *out = record.second_i32;
            return true;
        case IntegerField::kThirdI32:
            *out = record.third_i32;
            return true;
        case IntegerField::kFourthI32:
            *out = record.fourth_i32;
            return true;
        case IntegerField::kFifthI32:
            *out = record.fifth_i32;
            return true;
        case IntegerField::kSixthI32:
            *out = record.sixth_i32;
            return true;
        case IntegerField::kSeventhI32:
            *out = record.seventh_i32;
            return true;
    }
    return false;
}

}  // namespace

const char* field_name(IntegerField field) {
    switch (field) {
        case IntegerField::kFirstI32: return "first_i32";
        case IntegerField::kSecondI32: return "second_i32";
        case IntegerField::kThirdI32: return "third_i32";
        case IntegerField::kFourthI32: return "fourth_i32";
        case IntegerField::kFifthI32: return "fifth_i32";
        case IntegerField::kSixthI32: return "sixth_i32";
        case IntegerField::kSeventhI32: return "seventh_i32";
    }
    return "unknown";
}

bool parse_field_name(const std::string& name, IntegerField* out) {
    if (out == nullptr) return false;
    if (name == "first_i32") {
        *out = IntegerField::kFirstI32;
    } else if (name == "second_i32") {
        *out = IntegerField::kSecondI32;
    } else if (name == "third_i32") {
        *out = IntegerField::kThirdI32;
    } else if (name == "fourth_i32") {
        *out = IntegerField::kFourthI32;
    } else if (name == "fifth_i32") {
        *out = IntegerField::kFifthI32;
    } else if (name == "sixth_i32") {
        *out = IntegerField::kSixthI32;
    } else if (name == "seventh_i32") {
        *out = IntegerField::kSeventhI32;
    } else {
        return false;
    }
    return true;
}

Mapping reviewed_original_mapping() {
    Mapping mapping;
    mapping.identity_field = IntegerField::kFirstI32;
    mapping.base_health_field = IntegerField::kSecondI32;
    mapping.review_status = ReviewStatus::kReviewed;
    return mapping;
}

bool is_usable(const Mapping& mapping) {
    return mapping.review_status == ReviewStatus::kReviewed &&
        mapping.identity_field != mapping.base_health_field;
}

bool resolve_base_health(
        const std::vector<game_levels_global_section::EnemyRecord>& records,
        const Mapping& mapping,
        unsigned enemy_id,
        int* out_base_health) {
    if (out_base_health == nullptr || !is_usable(mapping)) return false;
    if (enemy_id > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }

    const std::int32_t wanted = static_cast<std::int32_t>(enemy_id);
    bool found = false;
    std::int32_t resolved_health = 0;

    for (const auto& record : records) {
        std::int32_t identity = 0;
        if (!read_field(record, mapping.identity_field, &identity)) return false;
        if (identity != wanted) continue;

        // A semantic identity field should select one imported data row. If it
        // selects more than one, fail closed instead of choosing by order.
        if (found) return false;

        if (!read_field(record, mapping.base_health_field, &resolved_health)) {
            return false;
        }
        found = true;
    }

    if (!found || resolved_health <= 0) return false;
    *out_base_health = static_cast<int>(resolved_health);
    return true;
}

std::optional<std::uint32_t> resolve_input_value_2bc(
        const std::vector<game_levels_global_section::EnemyRecord>& records,
        const Mapping& mapping,
        unsigned enemy_id) {
    if (!is_usable(mapping) ||
        enemy_id > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max())) {
        return std::nullopt;
    }

    const std::int32_t wanted = static_cast<std::int32_t>(enemy_id);
    const game_levels_global_section::EnemyRecord* match = nullptr;
    for (const auto& record : records) {
        std::int32_t identity = 0;
        if (!read_field(record, mapping.identity_field, &identity)) return std::nullopt;
        if (identity != wanted) continue;
        if (match != nullptr) return std::nullopt;
        match = &record;
    }

    if (match == nullptr) return std::nullopt;
    return enemy_attack_value_contract::try_initialize_value_2bc(match->first_float);
}

bool roster_base_health_resolver(
        unsigned enemy_id,
        int* out_base_health,
        void* user_data) {
    if (user_data == nullptr) return false;
    const auto* context = static_cast<const ResolverContext*>(user_data);
    if (context->mapping == nullptr || context->records == nullptr) return false;
    return resolve_base_health(
        *context->records,
        *context->mapping,
        enemy_id,
        out_base_health);
}

}  // namespace nevergone::game_levels_enemy_record_mapping
