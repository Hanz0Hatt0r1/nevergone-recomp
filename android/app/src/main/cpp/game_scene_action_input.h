#pragma once

#include <cstdint>
#include <optional>
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

    // C taps accepted while the interactive tutorial owns the Jump step. These
    // requests drive the reconstructed MEPlayer action loader, but do not by
    // themselves complete the lesson; the shipped LessonLayer advances only
    // once MEPLAYERSTATE actually reaches 0x20.
    std::uint64_t tutorial_jump_request_count = 0;

    // Evidence-backed tutorial transitions published by this boundary after a
    // corresponding live player state has been committed.
    std::uint64_t tutorial_event_count = 0;
};

void reset();

// Semantic action-control boundary. This intentionally does not own screen
// geometry. The recovered Controls hit-test layer calls this only after it has
// proven which original A/B/C control received the gesture.
bool observe(Control control, Gesture gesture);

// Observe a committed live MEPLAYERSTATE after its action document has been
// loaded. The original Jump lesson advances on state 0x20, not on the raw C
// tap. Returns true only when that exact state advances the live Jump step.
bool observe_live_meplayer_state(std::uint32_t meplayer_state);

// Recovered Controls::OnHandleButton(CONTROLSSTATE,bool) values for the three
// action buttons. The shipped function selects Controls +0x12c/+0x130/+0x134
// for state values 7/8/9 respectively.
std::optional<int> original_controls_state(Control control);
std::optional<Control> control_for_original_controls_state(int state);

Snapshot snapshot();
const char* control_name(Control control);
const char* gesture_name(Gesture gesture);
std::string status_report();

}  // namespace nevergone::game_scene_action_input
