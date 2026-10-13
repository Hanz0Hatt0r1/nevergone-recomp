#include <cassert>

#include "scene_actions_live_runtime.h"

int main() {
    namespace live = nevergone::scene_actions_live_runtime;
    namespace timed = nevergone::scene_actions_timed_spatial_trigger;

    timed::Result result;
    result.status = timed::Status::kPairTriggered;
    assert(live::timed_trigger_requests_start(result));

    result.status = timed::Status::kPairNotTriggered;
    assert(!live::timed_trigger_requests_start(result));
    result.status = timed::Status::kNoEvent;
    assert(!live::timed_trigger_requests_start(result));
    result.status = timed::Status::kSpatialRejected;
    assert(!live::timed_trigger_requests_start(result));
    result.status = timed::Status::kScheduleRejected;
    assert(!live::timed_trigger_requests_start(result));
    result.status = timed::Status::kNoPair;
    assert(!live::timed_trigger_requests_start(result));
    result.status = timed::Status::kAmbiguousPairs;
    assert(!live::timed_trigger_requests_start(result));
    result.status = timed::Status::kInvalidPair;
    assert(!live::timed_trigger_requests_start(result));
    result.status = timed::Status::kObjectMapMismatch;
    assert(!live::timed_trigger_requests_start(result));

    return 0;
}
