#include "player_combat_stat_contract.h"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace nevergone::player_combat_stat_contract {

namespace {

bool read_little_endian_float(
        std::string_view bytes, std::size_t offset, float* output) noexcept {
    if (output == nullptr || offset > bytes.size() || bytes.size() - offset < 4u) {
        return false;
    }
    const auto* source = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    const std::uint32_t bits = static_cast<std::uint32_t>(source[0]) |
        (static_cast<std::uint32_t>(source[1]) << 8u) |
        (static_cast<std::uint32_t>(source[2]) << 16u) |
        (static_cast<std::uint32_t>(source[3]) << 24u);
    std::memcpy(output, &bits, sizeof(bits));
    return std::isfinite(*output);
}

bool read_little_endian_u32(
        std::string_view bytes, std::size_t offset, std::uint32_t* output) noexcept {
    if (output == nullptr || offset > bytes.size() || bytes.size() - offset < 4u) {
        return false;
    }
    const auto* source = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    *output = static_cast<std::uint32_t>(source[0]) |
        (static_cast<std::uint32_t>(source[1]) << 8u) |
        (static_cast<std::uint32_t>(source[2]) << 16u) |
        (static_cast<std::uint32_t>(source[3]) << 24u);
    return true;
}

bool parse_integer_cell(std::string_view cell, std::int32_t* output) {
    if (output == nullptr) return false;
    const std::string text(cell);
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str()) return false;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0' || value < std::numeric_limits<std::int32_t>::min() ||
            value > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    *output = static_cast<std::int32_t>(value);
    return true;
}

}  // namespace

std::int32_t resolve_weapon_star_level(std::uint32_t hole_array_count) noexcept {
    return hole_array_count == 0
        ? 0
        : static_cast<std::int32_t>(hole_array_count - 1U);
}

bool parse_native_weapon_attack_file(
        std::string_view bytes, NativeWeaponAttackFileFields* output) noexcept {
    if (output == nullptr || bytes.size() < 80u) return false;

    NativeWeaponAttackFileFields fields;
    constexpr std::array<std::size_t, 6> attack_offsets{{
        28u, 32u, 36u, 40u, 44u, 48u,
    }};
    constexpr std::array<std::size_t, 6> scale_offsets{{
        56u, 60u, 64u, 68u, 72u, 76u,
    }};
    for (std::size_t i = 0; i < attack_offsets.size(); ++i) {
        if (!read_little_endian_float(
                    bytes, attack_offsets[i], &fields.base_attack_fields[i]) ||
                !read_little_endian_float(bytes, scale_offsets[i], &fields.scale_fields[i])) {
            return false;
        }
    }

    std::uint32_t range_value = 0u;
    if (!read_little_endian_u32(bytes, 52u, &range_value)) return false;
    if (range_value > 4u) {
        fields.range_low_84 = static_cast<float>(range_value - 5u);
        fields.range_high_88 = static_cast<float>(range_value + 5u);
    }

    *output = fields;
    return true;
}

std::optional<NativeWeaponAttackBuild> resolve_native_weapon_attack_build(
        const NativeWeaponAttackFileFields& fields,
        float quality_roll, float attack_roll) noexcept {
    if (!std::isfinite(fields.range_low_84) || !std::isfinite(fields.range_high_88) ||
            !std::isfinite(quality_roll) || !std::isfinite(attack_roll) ||
            quality_roll < 0.0f || quality_roll > 1.0f ||
            attack_roll < 0.0f || attack_roll > 1.0f) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < fields.base_attack_fields.size(); ++i) {
        if (!std::isfinite(fields.base_attack_fields[i]) ||
                !std::isfinite(fields.scale_fields[i])) {
            return std::nullopt;
        }
    }

    std::uint32_t quality_tier = 0u;
    std::int32_t minimum = 0;
    std::int32_t maximum = 5;
    if (quality_roll <= 0.05f) {
        quality_tier = 2u;
        minimum = 10;
        maximum = 10;
    } else if (quality_roll < 0.2f) {
        quality_tier = 1u;
        minimum = 5;
        maximum = 10;
    }

    const float minimum_value = static_cast<float>(minimum);
    const float maximum_value = static_cast<float>(maximum);
    constexpr std::size_t kBaseAddIndex = 5u;
    const float base_add_lower = fields.base_attack_fields[kBaseAddIndex] +
        fields.scale_fields[kBaseAddIndex] * minimum_value;
    const float base_add_upper = fields.base_attack_fields[kBaseAddIndex] +
        fields.scale_fields[kBaseAddIndex] * maximum_value;
    const float base_add = base_add_lower +
        (base_add_upper - base_add_lower) * attack_roll;

    NativeWeaponAttackBuild result;
    result.quality_tier = quality_tier;
    result.shared_attack_fraction = 1.0f;
    if (quality_tier == 2u) {
        for (std::size_t i = 0; i < result.attack_fields.size(); ++i) {
            result.attack_fields[i] = fields.base_attack_fields[i] +
                fields.scale_fields[i] * maximum_value;
        }
    } else {
        const float range = static_cast<float>(maximum - minimum);
        result.shared_attack_fraction = (base_add - base_add_lower) /
            (fields.scale_fields[kBaseAddIndex] * range);
        for (std::size_t i = 0; i < kBaseAddIndex; ++i) {
            const float lower = fields.base_attack_fields[i] +
                fields.scale_fields[i] * minimum_value;
            result.attack_fields[i] = lower + result.shared_attack_fraction *
                (fields.scale_fields[i] * range);
        }
        result.attack_fields[kBaseAddIndex] = base_add;
    }

    const float weapon_range = fields.range_high_88 - fields.range_low_84;
    result.weapon_curve_origin_8c =
        (fields.range_low_84 + fields.range_high_88) * 0.5f;
    result.weapon_curve_offset_90 = fields.base_attack_fields[0];
    result.weapon_curve_scale_94 =
        (fields.base_attack_fields[0] * -4.0f) / (weapon_range * weapon_range);
    return result;
}

float resolve_weapon_add_hole_base_attack(
        std::int32_t star_level, float weapon_field_7c) noexcept {
    if (star_level <= 1) return 0.0f;
    float value = static_cast<float>(star_level + 2) * weapon_field_7c;
    value *= static_cast<float>(star_level - 1);
    return 0.0f + value * 0.5f;
}

float resolve_weapon_add_temper_base_attack(
        std::int32_t weapon_field_ac, float weapon_field_7c) noexcept {
    const std::uint32_t shifted_bits =
        static_cast<std::uint32_t>(weapon_field_ac) << 1U;
    std::int32_t shifted_value = 0;
    std::memcpy(&shifted_value, &shifted_bits, sizeof(shifted_value));
    return static_cast<float>(shifted_value) * weapon_field_7c;
}

WeaponFinalAddBaseResult resolve_weapon_final_add_base_attack(
        const WeaponFinalAddBaseTerms& terms) noexcept {
    float value = 0.0f;
    if (terms.management_hole_gate_nonzero) {
        value = terms.add_hole_base_attack + value;
    }
    if (terms.weapon_field_18_is_zero) value += terms.add_temper_base_attack;
    value = value + terms.extra_base_attack + terms.weapon_field_60;
    const bool extra_flag_64 = terms.extra_base_attack > 0.0f;
    if (value < 0.0f) value = 0.0f;
    return WeaponFinalAddBaseResult{value, extra_flag_64};
}

WeaponFinalAddCategoryResult resolve_weapon_final_add_category_attack(
        const WeaponFinalAddCategoryTerms& terms) noexcept {
    float value = 0.0f;
    if (terms.weapon_field_18_is_zero) {
        value = terms.add_temper_attack + value;
    }
    if (terms.management_hole_gate_nonzero) {
        value = value + terms.add_hole_attack;
    }
    value = value + terms.extra_attack + terms.weapon_attack_field;
    if (value < 0.0f) value = 0.0f;
    return WeaponFinalAddCategoryResult{
        value,
        terms.extra_attack > 0.0f,
    };
}

WeaponFinalAddSuprResult resolve_weapon_final_add_supr_attack(
        const WeaponFinalAddSuprTerms& terms) noexcept {
    float value = 0.0f;
    if (terms.management_hole_gate_nonzero) {
        value = terms.add_hole_attack + value;
    }

    bool extra_flag_4c = terms.extra_attack > 0.0f;
    value = value + terms.extra_attack;

    bool special_flag_e2 = false;
    if (terms.weapon_field_18 == 0) {
        special_flag_e2 = terms.weapon_field_d4_is_zero &&
                terms.data_manager_54_field_4c_is_zero;
        value = value + terms.add_temper_attack;
    } else if (terms.weapon_field_18 == 1 && terms.weapon_field_d4_is_zero) {
        special_flag_e2 = true;
    }

    value = value + terms.weapon_field_48;
    if (value < 0.0f) value = 0.0f;
    if (special_flag_e2) {
        extra_flag_4c = false;
        value = 0.0f;
    }
    return WeaponFinalAddSuprResult{value, extra_flag_4c, special_flag_e2};
}

float resolve_weapon_add_category_temper_attack(
        bool category_temper_gate_clear, std::int32_t weapon_field_ac,
        float category_scale) noexcept {
    return category_temper_gate_clear
        ? resolve_weapon_add_temper_base_attack(weapon_field_ac, category_scale)
        : 0.0f;
}

float resolve_weapon_add_category_hole_attack(
        std::int32_t star_level, float category_scale) noexcept {
    return resolve_weapon_add_hole_base_attack(star_level, category_scale);
}

float resolve_weapon_add_category_attack(
        const WeaponCategoryAttackTerms& terms,
        std::uint32_t category_type) noexcept {
    float result = 0.0f;
    for (const auto& item : terms.weapon_items) {
        if (item.type == category_type) {
            if (terms.weapon_field_18 == 0 && item.weapon_name_matches) {
                result = result + item.matching_weapon_attack_addition;
            } else {
                result = result + item.regular_attack_addition;
            }
        } else if (item.type == 1000u) {
            for (const auto& nested : item.nested_modifiers) {
                if (nested.type != category_type) continue;
                if (terms.weapon_field_18 == 0) {
                    const float multiplier =
                        terms.data_manager_54_field_50 + 1.0f;
                    result = result +
                        nested.value_when_weapon_field_18_is_zero * multiplier;
                } else if (terms.weapon_field_18 == 1) {
                    result = result +
                        nested.value_when_weapon_field_18_is_one;
                }
            }
        }
    }
    return result;
}

float resolve_weapon_extra_category_attack(
        const WeaponCategoryExtraAttackTerms& terms) noexcept {
    float result = terms.weapon_add_attack + 0.0f;
    if (!terms.weapon_field_18_is_zero) return result;

    if (terms.use_management_level_effects) {
        for (const auto& effect : terms.management_effects) {
            if (effect.type != terms.category_type || effect.level == 0u) continue;
            result = result + effect.base_bonus;
            result = result +
                static_cast<float>(effect.level - 1u) * effect.growth_bonus;
        }
    } else {
        result = result + terms.parameter_attack_bonus;
    }

    result = result + terms.data_manager_54_field_38;
    return result + terms.equipment_property_bonus;
}

float resolve_attack(const State& player, const AttackTerms& terms) noexcept {
    return terms.weapon_base_attack_38 + terms.weapon_final_add_base_attack +
        terms.player_level_attack_110 + player.equipment_add_attack_114 +
        terms.player_status_value_144;
}

bool parse_player_level_attack_csv(
        std::string_view csv, std::vector<PlayerLevelAttackRow>* output) {
    if (output == nullptr) return false;

    std::vector<PlayerLevelAttackRow> rows;
    std::size_t line_start = 0;
    while (line_start < csv.size()) {
        const std::size_t newline = csv.find('\n', line_start);
        std::string_view line = csv.substr(
            line_start,
            newline == std::string_view::npos ? csv.size() - line_start : newline - line_start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) return false;

        std::array<std::string_view, 10> columns{};
        std::size_t column_start = 0;
        for (std::size_t column = 0; column < columns.size(); ++column) {
            const std::size_t comma = line.find(',', column_start);
            if (column + 1u < columns.size()) {
                if (comma == std::string_view::npos) return false;
                columns[column] = line.substr(column_start, comma - column_start);
                column_start = comma + 1u;
            } else {
                if (comma != std::string_view::npos) return false;
                columns[column] = line.substr(column_start);
            }
        }

        PlayerLevelAttackRow row;
        if (!parse_integer_cell(columns[8], &row.attack_increment_column_8)) return false;
        rows.push_back(row);

        if (newline == std::string_view::npos) break;
        line_start = newline + 1u;
    }
    if (rows.empty()) return false;
    output->swap(rows);
    return true;
}

std::optional<float> resolve_player_level_attack(
        std::uint32_t level,
        const std::vector<PlayerLevelAttackRow>& rows,
        float player_status_add_attack_120,
        float eat_attack_value) noexcept {
    if (level > rows.size()) return std::nullopt;

    float attack = 0.0f;
    for (std::uint32_t row = 1u; row < level; ++row) {
        attack = attack + static_cast<float>(rows[row].attack_increment_column_8);
    }
    attack = attack + player_status_add_attack_120;
    return attack + eat_attack_value;
}

float resolve_damage_absorb(
        const State& player, const DamageAbsorbTerms& terms) noexcept {
    float equipment_absorb = 0.0f;
    for (const auto& effect : terms.equipment_effects) {
        if (effect.type == 0x1cu) equipment_absorb = effect.value;
    }

    float multiplier_bonus = terms.parameter_multiplier_bonus_308;
    if (terms.use_management_level_effects) {
        multiplier_bonus = 0.0f;
        for (const auto& effect : terms.management_effects) {
            if (effect.type != 10u) continue;
            multiplier_bonus = multiplier_bonus + effect.base_bonus;
            multiplier_bonus = multiplier_bonus +
                static_cast<float>(effect.level - 1u) * effect.growth_bonus;
        }
    }

    float attribute_absorb = 0.0f;
    for (const auto& effect : terms.attribute_effects) {
        if ((effect.type == 2u || effect.type == 4u) && effect.enabled) {
            attribute_absorb +=
                (static_cast<float>(effect.level) * effect.per_level_bonus) / 100.0f;
        }
    }

    const float armband_multiplier = terms.armband_equipped
        ? terms.armband_multiplier_40 : 0.0f;
    const float armband_absorb = terms.armband_equipped
        ? terms.armband_absorb_addition / 100.0f : 0.0f;
    const float absorb_sum = terms.player_level_absorb + equipment_absorb +
        armband_absorb + terms.player_status_absorb_140 +
        player.equipment_add_damage_absorb_118 + attribute_absorb;
    const float multiplier = multiplier_bonus + 1.0f + armband_multiplier;
    return absorb_sum * multiplier;
}

float resolve_max_health(State* player, const MaxHealthTerms& terms) noexcept {
    if (player == nullptr) return 0.0f;

    float management_bonus = terms.parameter_bonus_304;
    if (terms.use_management_level_effects) {
        management_bonus = 0.0f;
        for (const auto& effect : terms.management_effects) {
            if (effect.type != 9u || effect.level == 0u) continue;
            management_bonus = management_bonus + effect.base_bonus;
            management_bonus = management_bonus +
                static_cast<float>(effect.level - 1u) * effect.growth_bonus;
        }
    }

    float attribute_bonus = 0.0f;
    for (const auto& effect : terms.attribute_effects) {
        if ((effect.type == 7u || effect.type == 8u) && effect.enabled) {
            attribute_bonus = attribute_bonus +
                static_cast<float>(effect.level) * effect.per_level_bonus;
        }
    }

    const float armband_multiplier = terms.armband_equipped
        ? terms.armband_health_multiplier_3c : 0.0f;
    const float armband_addition = terms.armband_equipped
        ? static_cast<float>(terms.armband_health_addition) : 0.0f;
    const float mercenary_bonus = terms.has_living_type6_mercenary
        ? static_cast<float>(terms.type6_mercenary_health) : 0.0f;

    float health_sum = terms.player_level_health + 100.0f;
    health_sum = health_sum + mercenary_bonus;
    health_sum = health_sum + static_cast<float>(terms.weapon_add_health);
    health_sum = health_sum + player->tools_add_health_120;
    health_sum = health_sum + management_bonus;
    health_sum = health_sum + attribute_bonus;
    health_sum = health_sum + armband_addition;

    const float health_multiplier = armband_multiplier + 1.0f;
    const float modified_health = health_sum * health_multiplier;
    float result = player->equipment_add_health_110 + modified_health;
    result = result + terms.player_status_health_138;

    player->effective_max_health_11c = static_cast<std::int32_t>(result);
    return static_cast<float>(player->effective_max_health_11c);
}

float resolve_extra_base_attack(const ExtraBaseAttackTerms& terms) noexcept {
    float result = terms.weapon_add_base_attack + 0.0f;
    if (!terms.weapon_field_18_is_zero) return result;

    if (terms.use_management_level_effects) {
        for (const auto& effect : terms.management_effects) {
            if (effect.type != 3u || effect.level == 0u) continue;
            result = result + effect.base_bonus;
            result = result +
                static_cast<float>(effect.level - 1u) * effect.growth_bonus;
        }
    } else {
        result = result + terms.parameter_bonus_2ec;
    }

    for (const auto& effect : terms.attribute_effects) {
        if ((effect.type == 5u || effect.type == 6u || effect.type == 10u) &&
            effect.enabled) {
            result = result +
                static_cast<float>(effect.level) * effect.per_level_bonus;
        }
    }

    if (terms.armband_equipped) {
        result = result + static_cast<float>(terms.armband_attack_addition);
        result = result + terms.armband_base_attack_38;
    }
    return result + terms.equipment_property_bonus_30;
}

float resolve_weapon_add_base_attack(const WeaponBaseAttackTerms& terms) noexcept {
    float result = 0.0f;
    for (const auto& item : terms.weapon_items) {
        if (item.type == 0x1eu) {
            if (terms.weapon_field_18 == 0 && item.weapon_name_matches) {
                result = result + item.matching_weapon_attack_addition;
            } else {
                result = result + item.regular_attack_addition;
            }
        } else if (item.type == 1000u) {
            for (const auto& nested : item.nested_modifiers) {
                if (nested.type != 0x1eu) continue;
                if (terms.weapon_field_18 == 0) {
                    const float multiplier =
                        terms.armband_effect_multiplier_50 + 1.0f;
                    result = result +
                        nested.value_when_weapon_field_18_is_zero * multiplier;
                } else if (terms.weapon_field_18 == 1) {
                    result = result + nested.value_when_weapon_field_18_is_one;
                }
            }
        }
    }
    return result;
}

void add_cloth_attack_value(
        PlayerStatusInfoState* status, bool management_gate_nonzero,
        float value) noexcept {
    if (status == nullptr || management_gate_nonzero) return;
    status->cloth_add_attack_144 = status->cloth_add_attack_144 + value;
}

void add_cloth_health_value(
        PlayerStatusInfoState* status, bool management_gate_nonzero,
        float value) noexcept {
    if (status == nullptr || management_gate_nonzero) return;
    status->cloth_add_health_138 = status->cloth_add_health_138 + value;
}

void add_cloth_strength_value(
        PlayerStatusInfoState* status, bool management_gate_nonzero,
        float value) noexcept {
    if (status == nullptr || management_gate_nonzero) return;
    status->cloth_add_strength_13c = status->cloth_add_strength_13c + value;
}

void add_cloth_damage_absorb_value(
        PlayerStatusInfoState* status, bool management_gate_nonzero,
        float value) noexcept {
    if (status == nullptr || management_gate_nonzero) return;
    status->cloth_add_damage_absorb_140 =
        status->cloth_add_damage_absorb_140 + value;
}

void reset_cloth_add_values(PlayerStatusInfoState* status) noexcept {
    if (status == nullptr) return;
    status->cloth_add_health_138 = 0.0f;
    status->cloth_add_strength_13c = 0.0f;
    status->cloth_add_damage_absorb_140 = 0.0f;
    status->cloth_add_attack_144 = 0.0f;
}

float resolve_eat_attribute(
        std::int32_t type, std::int32_t tier,
        const std::vector<EatAttributeCsvRow>& rows) noexcept {
    for (const auto& row : rows) {
        if (row.tier != tier || row.type != type) continue;
        switch (type) {
            case 1: return row.column_3;
            case 2: return row.column_4;
            case 3: return row.column_5;
            case 4: return row.column_6;
            default: return 0.0f;
        }
    }
    return 0.0f;
}

float resolve_hero_eat_attribute(
        std::int32_t type, std::uint32_t count,
        const std::vector<EatAttributeCsvRow>& rows) noexcept {
    std::int32_t csv_type = type;
    bool apply_hundredth_scale = false;
    switch (type) {
        case 1:
        case 2:
        case 4:
            break;
        case 5:
            csv_type = 3;
            apply_hundredth_scale = true;
            break;
        default:
            return 0.0f;
    }

    const std::uint32_t million_count = count / 1000000u;
    const std::uint32_t remainder = count % 1000000u;
    const std::uint32_t thousand_count = remainder / 1000u;
    const std::uint32_t unit_count = remainder % 1000u;
    float result = static_cast<float>(million_count) *
        resolve_eat_attribute(csv_type, 3, rows);
    result = result + static_cast<float>(thousand_count) *
        resolve_eat_attribute(csv_type, 2, rows);
    result = result + static_cast<float>(unit_count) *
        resolve_eat_attribute(csv_type, 1, rows);
    return apply_hundredth_scale ? result * 0.01f : result;
}

void add_eat_property(
        PlayerStatusInfoState* status, const EatPropertyTerms& terms) noexcept {
    if (status == nullptr || terms.management_gate_nonzero) return;

    float* target = nullptr;
    float limit = 0.0f;
    switch (terms.property_type) {
        case 1:
            target = &status->eat_property_148;
            limit = 2000.0f;
            break;
        case 2:
            target = &status->eat_property_154;
            limit = 2000.0f;
            break;
        case 4:
            target = &status->eat_property_14c;
            limit = 200.0f;
            break;
        case 5:
            target = &status->eat_property_150;
            limit = 200.0f;
            break;
        default:
            return;
    }
    const std::uint32_t count = *target > 0.0f
        ? static_cast<std::uint32_t>(static_cast<std::int32_t>(*target))
        : 0u;
    const float current_bonus = resolve_hero_eat_attribute(
        terms.property_type, count, terms.attribute_rows);
    if (current_bonus < limit) {
        *target = *target + terms.addition;
    }
}

void set_equipment_add_attack(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->equipment_add_attack_114 = value;
}

void set_equipment_add_health(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->equipment_add_health_110 = value;
}

void add_equipment_add_health(State* state, float delta) noexcept {
    if (state == nullptr) return;
    state->equipment_add_health_110 += delta;
}

void set_equipment_add_damage_absorb(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->equipment_add_damage_absorb_118 = value;
}

void set_tools_add_health(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->tools_add_health_120 = value;
}

void set_tools_damage_absorb(State* state, float value) noexcept {
    if (state == nullptr) return;
    state->tools_damage_absorb_factor_430 = 1.0f - value;
}

}  // namespace nevergone::player_combat_stat_contract
