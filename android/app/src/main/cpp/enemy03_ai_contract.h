#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "enemy_hit_gate_contract.h"

namespace nevergone::enemy03_ai_contract {

struct Inputs {
    std::int32_t random_gate = 0;
    std::int32_t candidate_state = 0;
    std::uint32_t current_state = 0;
    std::int32_t horizontal_distance = 0;
    std::int32_t relative_facing = 0;
    std::int32_t unused_param7 = 0;
    std::int32_t enemy_timer = 0;
    std::int32_t candidate_range_max = 0;
    std::int32_t candidate_range_min = 0;
    std::int32_t player_state = 0;
};

// enemy03_AI returns these integer codes; the caller consumes values 1..6.
enum class Decision : std::int32_t {
    kContinue = 0,
    kSelectCandidate = 1,
    kRetarget = 2,
    kSpecial3 = 3,
    kSpecial4 = 4,
    kSpecial5 = 5,
    kSpecial6 = 6,
};

struct CandidateDecision {
    std::size_t candidate_index = 0;
    std::int32_t candidate_state = 0;
    Decision decision = Decision::kContinue;
};

// eventAction scales lrand48() through the original 2^-31 and 10000.0f literals.
std::int32_t random_gate_from_lrand48(std::int32_t value) noexcept;

Decision decide(const Inputs& inputs) noexcept;

// eventAction scans candidate indices in order and calls enemy03_AI only when
// checkInputStateIsBeaten() rejects that candidate.
std::optional<CandidateDecision> first_candidate_decision(
        const std::vector<Inputs>& candidates,
        bool suppress_states_5_6,
        bool lesson_step6_no_progress) noexcept;

// EnemyObject::eventAction accumulates delta and consumes at most one second
// per invocation. Returns false for subsecond and unordered (NaN) totals.
bool event_interval_elapsed(float delta, float* accumulated_seconds) noexcept;

// EnemyObject::enemy03_Phy adjusts the integer AI timer on a state change.
std::uint32_t timer_after_state_change(
        std::uint32_t current_timer, std::uint32_t new_state) noexcept;

// EnemyObject::enemyPhysical(int) advances that timer and clamps it to [0,1000].
std::uint32_t advance_timer(std::uint32_t current_timer, std::int32_t delta) noexcept;

}  // namespace nevergone::enemy03_ai_contract
