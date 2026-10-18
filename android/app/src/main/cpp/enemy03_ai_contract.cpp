#include "enemy03_ai_contract.h"

namespace nevergone::enemy03_ai_contract {
namespace {

bool negative(std::uint32_t value) noexcept {
    return (value & 0x80000000u) != 0;
}

std::uint32_t clamp_nonnegative(std::uint32_t value) noexcept {
    return negative(value) ? 0u : value;
}

}  // namespace

std::int32_t random_gate_from_lrand48(std::int32_t value) noexcept {
    const float normalized = static_cast<float>(value) * 0x1p-31f;
    return static_cast<std::int32_t>(normalized * 10000.0f);
}

Decision decide(const Inputs& in) noexcept {
    if (in.current_state == 0x1cu) return Decision::kContinue;
    if (in.current_state == 0x2fu) {
        return in.candidate_state == 0x1c && in.horizontal_distance < 300
                ? Decision::kSelectCandidate
                : Decision::kContinue;
    }

    if (in.random_gate < 8000) {
        if (in.player_state == 2 || in.player_state == 0x15) {
            if (in.candidate_state == 0x2b && in.relative_facing == 1 &&
                in.horizontal_distance < 300) {
                return Decision::kSpecial6;
            }
            if (in.candidate_state == 0x24 && in.horizontal_distance < 0x96) {
                return Decision::kSelectCandidate;
            }
        } else if (in.player_state == 0x20 ||
                   static_cast<std::uint32_t>(in.player_state) - 0x19u < 2u) {
            if (in.candidate_state == 0x26 && in.relative_facing == 1 &&
                in.horizontal_distance < 200) {
                return Decision::kSelectCandidate;
            }
        } else if ((in.candidate_state == 0x26 || in.candidate_state == 0x25 ||
                    in.candidate_state == 2 || in.candidate_state == 3) &&
                   in.horizontal_distance < in.candidate_range_max &&
                   in.candidate_range_min < in.horizontal_distance &&
                   in.enemy_timer > 200) {
            return Decision::kSelectCandidate;
        }
    }

    if ((in.current_state < 2u || in.current_state == 0x14u) &&
        in.horizontal_distance < 0x118 && in.candidate_state == 0x1b) {
        if (in.random_gate < 5000) return Decision::kSpecial5;
        if (in.random_gate < 7000) return Decision::kSpecial3;
        return Decision::kSpecial4;
    }

    if (in.candidate_state == 1) {
        return in.horizontal_distance < 0x12d
                ? Decision::kContinue
                : Decision::kRetarget;
    }
    return Decision::kContinue;
}

std::uint32_t timer_after_state_change(
        std::uint32_t current_timer, std::uint32_t new_state) noexcept {
    if (new_state == 2u || new_state == 3u) {
        current_timer -= 200u;
    } else if (new_state == 0x25u || new_state == 0x26u || new_state == 0x28u) {
        current_timer -= 300u;
    }
    return clamp_nonnegative(current_timer);
}

std::uint32_t advance_timer(std::uint32_t current_timer, std::int32_t delta) noexcept {
    const auto next = current_timer + static_cast<std::uint32_t>(delta);
    if (negative(next) || next == 0u) return 0u;
    return next < 1000u ? next : 1000u;
}

}  // namespace nevergone::enemy03_ai_contract
