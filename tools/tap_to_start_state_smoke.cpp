#include <cassert>
#include <cstdint>

#include "tap_to_start_state.h"

using nevergone::tap_to_start_state::Phase;

int main() {
    using namespace nevergone::tap_to_start_state;

    reset();
    auto state = snapshot();
    assert(state.phase == Phase::kInactive);
    assert(state.scene_generation == 0);
    assert(!state.auto_login_request_pending);
    assert(state.accepted_touches == 0);
    assert(!touch_began());

    sync_scene(1, false);
    assert(snapshot().phase == Phase::kInactive);
    assert(!touch_began());

    // The modern importer completes the obsolete OBB extraction/update stage
    // before the recovered scene begins, so an active SingleLogin generation
    // enters the original interactable state 4 directly.
    sync_scene(1, true);
    state = snapshot();
    assert(state.phase == Phase::kReady);
    assert(state.scene_generation == 1);

    assert(touch_began());
    state = snapshot();
    assert(state.phase == Phase::kAutoLoginPending);
    assert(state.auto_login_request_pending);
    assert(state.accepted_touches == 1);

    // Original OnTapScreen changes state 4 -> 5 before invoking the Android
    // SDK, therefore repeat touch-begins cannot submit another login request.
    assert(!touch_began());
    assert(snapshot().accepted_touches == 1);

    assert(consume_auto_login_request());
    assert(!consume_auto_login_request());
    state = snapshot();
    assert(state.phase == Phase::kAutoLoginPending);
    assert(!state.auto_login_request_pending);

    // Re-sampling the same active scene must not make state 5 interactive.
    sync_scene(1, true);
    assert(snapshot().phase == Phase::kAutoLoginPending);
    assert(!touch_began());

    // A recovered-scene restart gets a fresh generation and a fresh state-4
    // TapToStart gate.
    sync_scene(2, true);
    state = snapshot();
    assert(state.phase == Phase::kReady);
    assert(state.scene_generation == 2);
    assert(!state.auto_login_request_pending);
    assert(touch_began());
    assert(snapshot().accepted_touches == 2);

    sync_scene(2, false);
    state = snapshot();
    assert(state.phase == Phase::kInactive);
    assert(!state.auto_login_request_pending);
    assert(!touch_began());

    reset();
    state = snapshot();
    assert(state.phase == Phase::kInactive);
    assert(state.scene_generation == 0);
    assert(state.accepted_touches == 0);
    return 0;
}
