#include <cassert>
#include <cmath>
#include <limits>

#include "scene_actions_trigger_schedule.h"

int main() {
    namespace schedule = nevergone::scene_actions_trigger_schedule;

    static_assert(schedule::kEvents.size() == 8u);
    static_assert(schedule::kEvents[0].event_id == 2001);
    static_assert(schedule::kEvents[7].event_id == 2008);

    schedule::State state;

    // Native compares strictly with >. Equality with the first threshold does
    // not dispatch event 2001.
    auto result = schedule::advance(5.0f, &state);
    assert(result.status == schedule::Status::kNoEvent);
    assert(state.accumulated_seconds_240 == 5.0f);
    assert(!state.fired_244_24b[0]);

    result = schedule::advance(0.001f, &state);
    assert(result.status == schedule::Status::kEventReady);
    assert(result.event_id.has_value() && *result.event_id == 2001);
    assert(result.event_index.has_value() && *result.event_index == 0u);
    assert(state.fired_244_24b[0]);

    // One invocation can dispatch at most one new event even if the accumulator
    // crosses every remaining threshold.
    result = schedule::advance(100.0f, &state);
    assert(result.status == schedule::Status::kEventReady);
    assert(*result.event_id == 2002);
    for (std::size_t index = 2; index < schedule::kEvents.size(); ++index) {
        assert(!state.fired_244_24b[index]);
    }

    // With the accumulator already high, zero-dt invocations release the next
    // unfired event in native check order, one per call.
    for (int expected = 2003; expected <= 2008; ++expected) {
        result = schedule::advance(0.0f, &state);
        assert(result.status == schedule::Status::kEventReady);
        assert(result.event_id.has_value() && *result.event_id == expected);
    }
    result = schedule::advance(0.0f, &state);
    assert(result.status == schedule::Status::kNoEvent);

    // Reset restores both the accumulator and all one-shot flags.
    schedule::reset(&state);
    assert(state.accumulated_seconds_240 == 0.0f);
    for (bool fired : state.fired_244_24b) assert(!fired);

    // The native routine does not reject negative dt. Preserve the finite add
    // semantics rather than silently clamping to zero.
    result = schedule::advance(6.0f, &state);
    assert(result.status == schedule::Status::kEventReady);
    assert(*result.event_id == 2001);
    result = schedule::advance(-1.0f, &state);
    assert(result.status == schedule::Status::kNoEvent);
    assert(std::fabs(state.accumulated_seconds_240 - 5.0f) < 0.0001f);

    // Project boundary rejects non-finite values transactionally.
    const float before = state.accumulated_seconds_240;
    result = schedule::advance(std::numeric_limits<float>::quiet_NaN(), &state);
    assert(result.status == schedule::Status::kRejected);
    assert(state.accumulated_seconds_240 == before);

    return 0;
}
