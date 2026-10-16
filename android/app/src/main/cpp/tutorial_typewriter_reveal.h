#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace nevergone::tutorial_typewriter_reveal {

inline std::size_t visible_characters(
        std::size_t character_count,
        std::uint64_t elapsed_ticks,
        double reveal_seconds,
        double fixed_step_seconds) {
    if (character_count == 0) return 0;
    if (!std::isfinite(reveal_seconds) || reveal_seconds <= 0.0 ||
            !std::isfinite(fixed_step_seconds) || fixed_step_seconds <= 0.0) {
        return character_count;
    }
    const double progress = std::min(
        1.0,
        static_cast<double>(elapsed_ticks) * fixed_step_seconds / reveal_seconds);
    return std::min(
        character_count,
        static_cast<std::size_t>(std::floor(progress * character_count + 1e-9)));
}

}  // namespace nevergone::tutorial_typewriter_reveal
