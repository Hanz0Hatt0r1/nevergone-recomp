#pragma once

#include <cstdint>

namespace nevergone::tap_to_start_state {

// Recovered TapToStart interaction states from the original client.
// Legacy OBB extraction/update states 1..3 are intentionally handled by the
// modern importer before this scene becomes interactive.
enum class Phase : int {
    kInactive = 0,
    kReady = 4,
    kAutoLoginPending = 5,
};

struct Snapshot {
    Phase phase = Phase::kInactive;
    std::uint64_t scene_generation = 0;
    bool auto_login_request_pending = false;
    std::uint64_t accepted_touches = 0;
};

void reset();

// Synchronizes the reconstructed TapToStart overlay with the recovered scene
// generation. Once SingleLogin is active, the modern pre-imported resource
// path enters the original interactable state 4 directly.
void sync_scene(std::uint64_t scene_generation, bool single_login_active);

// Mirrors TapToStart::ccTouchBegan -> OnTapScreen. Returns true only when the
// original state-4 guard accepts the touch and advances to state 5.
bool touch_began();

// Exposes the original platformAutoLogin boundary to the next compatibility
// layer without inventing a network/login implementation in this module.
bool consume_auto_login_request();

Snapshot snapshot();

}  // namespace nevergone::tap_to_start_state
