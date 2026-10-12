#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nevergone::scene_actions_trigger_schedule {

// GameScene::TriggerSceneAction(float) retains an accumulated time value at
// GameScene +0x240 and eight one-shot bytes at +0x244..+0x24b.  The shipped
// ARMv7 control flow checks the thresholds in this exact order and returns as
// soon as one newly eligible event has been dispatched.
struct Event {
    float threshold_seconds = 0.0f;
    std::int32_t event_id = 0;
};

constexpr std::array<Event, 8> kEvents{{
    {5.0f, 2001},
    {5.400000095367431640625f, 2002},
    {5.80000019073486328125f, 2003},
    {6.19999980926513671875f, 2004},
    {6.599999904632568359375f, 2005},
    {7.0f, 2006},
    {7.400000095367431640625f, 2007},
    {7.80000019073486328125f, 2008},
}};

struct State {
    float accumulated_seconds_240 = 0.0f;
    std::array<bool, kEvents.size()> fired_244_24b{};
};

enum class Status {
    kNoEvent = 0,
    kEventReady,
    kRejected,
};

struct Result {
    Status status = Status::kNoEvent;
    std::optional<std::int32_t> event_id;
    std::optional<std::size_t> event_index;
};

// Advance one native TriggerSceneAction(float) invocation.
//
// Important recovered details:
// - dt is added before any threshold test;
// - the comparison is strict accumulated > threshold (native BLE skips equal);
// - already-fired thresholds are skipped in the same call;
// - once one new threshold fires, the function exits, so a large accumulated
//   value releases at most one new event per invocation.
//
// Non-finite input is rejected by the project boundary so NaN/Inf cannot poison
// persistent state. Finite negative input is retained because the native
// routine performs an unconditional floating-point add and has no sign check.
inline Result advance(float dt_seconds, State* state) {
    if (state == nullptr || !std::isfinite(dt_seconds) ||
            !std::isfinite(state->accumulated_seconds_240)) {
        return {Status::kRejected, std::nullopt, std::nullopt};
    }

    const float accumulated = state->accumulated_seconds_240 + dt_seconds;
    if (!std::isfinite(accumulated)) {
        return {Status::kRejected, std::nullopt, std::nullopt};
    }
    state->accumulated_seconds_240 = accumulated;

    for (std::size_t index = 0; index < kEvents.size(); ++index) {
        if (!(accumulated > kEvents[index].threshold_seconds) ||
                state->fired_244_24b[index]) {
            continue;
        }
        state->fired_244_24b[index] = true;
        return {Status::kEventReady, kEvents[index].event_id, index};
    }
    return {Status::kNoEvent, std::nullopt, std::nullopt};
}

inline void reset(State* state) {
    if (state != nullptr) *state = {};
}

}  // namespace nevergone::scene_actions_trigger_schedule
