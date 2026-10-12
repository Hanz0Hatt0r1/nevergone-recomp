#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace nevergone::game_scene_type0_action_pair {

// GameSceneObject::initWithData() exact-name branch for klhuo-1.png.
// The shipped ARMv7 code converts a non-negative 31-bit PRNG draw to float
// using VCVT.F32.S32 #31, multiplies by 3.0f, truncates to int, then adds 1.
// Keep this float32 path rather than replacing it with integer modulo/ranges:
// near INT31_MAX the conversion rounds to 1.0f and can therefore yield 4.
inline std::optional<int> variant_from_random31(std::uint32_t raw) {
    if (raw > 0x7fffffffu) return std::nullopt;
    const float normalized = static_cast<float>(static_cast<std::int32_t>(raw)) /
            2147483648.0f;
    const float scaled = normalized * 3.0f;
    if (!std::isfinite(scaled) || scaled < 0.0f) return std::nullopt;
    const int variant = static_cast<int>(scaled) + 1;
    if (variant < 1 || variant > 4) return std::nullopt;
    return variant;
}

struct Entry {
    std::string action_file;
    // SceneActionsSystem +0x24e is written immediately after construction.
    std::uint8_t mode_24e = 0;
};

struct Plan {
    int variant = 0;
    Entry born;
    Entry idle;
};

inline std::optional<Plan> build(std::uint32_t raw_random31) {
    const auto variant = variant_from_random31(raw_random31);
    if (!variant.has_value()) return std::nullopt;

    char born_name[32] = {};
    char idle_name[32] = {};
    const int born_written = std::snprintf(
            born_name, sizeof(born_name), "se01_born%02d.actData", *variant);
    const int idle_written = std::snprintf(
            idle_name, sizeof(idle_name), "se01_i%02d.actData", *variant);
    if (born_written <= 0 || idle_written <= 0 ||
            static_cast<std::size_t>(born_written) >= sizeof(born_name) ||
            static_cast<std::size_t>(idle_written) >= sizeof(idle_name)) {
        return std::nullopt;
    }

    Plan plan;
    plan.variant = *variant;
    // Original construction order is born first, then idle. The native branch
    // writes +0x24e = 0 on born and +0x24e = 1 on idle before adding both to the
    // owning GameSceneObject action-system array.
    plan.born.action_file = born_name;
    plan.born.mode_24e = 0u;
    plan.idle.action_file = idle_name;
    plan.idle.mode_24e = 1u;
    return plan;
}

}  // namespace nevergone::game_scene_type0_action_pair
