#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "game_levels_enemy_record_mapping.h"

int main() {
    using nevergone::game_levels_enemy_record_mapping::IntegerField;
    using nevergone::game_levels_enemy_record_mapping::Mapping;
    using nevergone::game_levels_enemy_record_mapping::ResolverContext;
    using nevergone::game_levels_enemy_record_mapping::ReviewStatus;
    namespace mapping = nevergone::game_levels_enemy_record_mapping;
    using EnemyRecord = nevergone::game_levels_global_section::EnemyRecord;

    IntegerField parsed = IntegerField::kFirstI32;
    assert(mapping::parse_field_name("first_i32", &parsed));
    assert(parsed == IntegerField::kFirstI32);
    assert(mapping::parse_field_name("seventh_i32", &parsed));
    assert(parsed == IntegerField::kSeventhI32);
    assert(!mapping::parse_field_name("first_float", &parsed));
    assert(!mapping::parse_field_name("enemy_id", &parsed));
    assert(mapping::field_name(IntegerField::kFourthI32) == std::string("fourth_i32"));

    const Mapping reviewed = mapping::reviewed_original_mapping();
    assert(reviewed.identity_field == IntegerField::kFirstI32);
    assert(reviewed.base_health_field == IntegerField::kSecondI32);
    assert(reviewed.review_status == ReviewStatus::kReviewed);
    assert(mapping::is_usable(reviewed));

    Mapping candidate = reviewed;
    candidate.review_status = ReviewStatus::kCandidate;
    assert(!mapping::is_usable(candidate));

    Mapping aliased = reviewed;
    aliased.base_health_field = aliased.identity_field;
    assert(!mapping::is_usable(aliased));

    std::vector<EnemyRecord> records(3);
    records[0].first_i32 = 3;
    records[0].second_i32 = 70;
    records[1].first_i32 = 21;
    records[1].second_i32 = 250;
    records[2].first_i32 = 99;
    records[2].second_i32 = 900;

    // Populate unrelated fields with plausible values so the smoke proves the
    // production helper selects the exact first/second slots rather than a
    // magnitude-based heuristic.
    records[0].third_i32 = 7000;
    records[0].fourth_i32 = 7001;
    records[1].third_i32 = 25000;
    records[1].fourth_i32 = 25001;

    int hp = -1;
    assert(mapping::resolve_base_health(records, reviewed, 3, &hp));
    assert(hp == 70);
    assert(mapping::resolve_base_health(records, reviewed, 21, &hp));
    assert(hp == 250);
    assert(!mapping::resolve_base_health(records, reviewed, 12345, &hp));
    assert(!mapping::resolve_base_health(records, candidate, 3, &hp));
    assert(!mapping::resolve_base_health(records, aliased, 3, &hp));
    assert(!mapping::resolve_base_health(records, reviewed, 3, nullptr));
    assert(!mapping::resolve_base_health(
        records,
        reviewed,
        static_cast<unsigned>(std::numeric_limits<std::int32_t>::max()) + 1u,
        &hp));

    ResolverContext context{&reviewed, &records};
    hp = -1;
    assert(mapping::roster_base_health_resolver(3, &hp, &context));
    assert(hp == 70);
    assert(!mapping::roster_base_health_resolver(3, &hp, nullptr));

    // Duplicate semantic identities are ambiguous and must not be selected by
    // physical record order.
    auto duplicate_records = records;
    duplicate_records.push_back(records[0]);
    duplicate_records.back().second_i32 = 71;
    assert(!mapping::resolve_base_health(duplicate_records, reviewed, 3, &hp));

    auto zero_health_records = records;
    zero_health_records[0].second_i32 = 0;
    assert(!mapping::resolve_base_health(zero_health_records, reviewed, 3, &hp));

    auto negative_health_records = records;
    negative_health_records[0].second_i32 = -7;
    assert(!mapping::resolve_base_health(negative_health_records, reviewed, 3, &hp));

    std::cout << "GameLevels enemy record mapping smoke: PASS\n";
    return 0;
}
