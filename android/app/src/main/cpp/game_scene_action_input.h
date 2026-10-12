#pragma once

#include <cstdint>
#include <string>

namespace nevergone::game_scene_action_input {

enum class Control : int {
    kNone = 0,
    kButtonA = 1,
    kButtonB = 2,
    kButtonC = 3,
};

enum class Gesture : int {
    kNone = 0,
    kPress = 1,
    kRelease = 2,
    kTap = 3,
    kHoldBegin = 4,
    kHoldEnd = 5,
};

struct Snapshot {
    Control last_control = Control::kNone;
    Gesture last_gesture = Gesture::kNone;
    std::uint64_t accepted_input_count = 0;
    std::uint64_t rejected_input_count = 0;
    std::uint64_t tutorial_event_count = 0;
};

void reset();

// Semantic action-control boundary. This intentionally does not own screen
// geometry. A future recovered Controls hit-test layer should call this only
// after it has proven which original A/B/C control received the gesture.
bool observe(Control control, Gesture gesture);

Snapshot snapshot();
const char* control_name(Control control);
const char* gesture_name(Gesture gesture);
std::string status_report();

}  // namespace nevergone::game_scene_action_input
