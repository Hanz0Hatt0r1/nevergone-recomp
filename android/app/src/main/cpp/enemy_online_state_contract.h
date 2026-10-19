#pragma once

#include <cstdint>

namespace nevergone::enemy_online_state_contract {

inline constexpr std::uint32_t kNoQueuedState = 0x2f;

struct State {
    std::uint32_t current_state = 0;
    std::uint32_t queued_state = kNoQueuedState;
    float health = 0.0f;
    bool suppress_states_5_6 = false;
};

struct Request {
    std::uint32_t requested_state = 0;
    bool force = false;
    std::int32_t current_action_frame = 0;
    bool current_action_done = true;
};

enum class Outcome {
    kInvalidState,
    kApplied,
    kIgnoredSentinel,
    kUnchanged,
    kAttackMiss,
    kActionStillRunning,
    kBeatenStateLocked,
};

struct Result {
    Outcome outcome = Outcome::kInvalidState;
    std::uint32_t requested_state = 0;
    std::uint32_t resolved_state = 0;
    bool clear_global_enemy_state_flag = false;
};

// Reconstructs EnemyObject::onlineEnemyState's request gates and state writes.
// Action selection, animation, sound, bounds and physics callbacks remain at
// the caller boundary.
Result request_state(State* state, const Request& request) noexcept;

}  // namespace nevergone::enemy_online_state_contract
