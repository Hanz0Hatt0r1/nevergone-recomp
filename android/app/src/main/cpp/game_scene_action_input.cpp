#include "game_scene_action_input.h"

#include <mutex>
#include <sstream>

#include "tutorial_gameplay_state.h"
#include "tutorial_session_state.h"

namespace nevergone::game_scene_action_input {
namespace {

std::mutex g_mutex;
Snapshot g_state{};

bool valid(Control control, Gesture gesture) {
    return control != Control::kNone && gesture != Gesture::kNone;
}

bool publish_tutorial_event(Control control, Gesture gesture) {
    const auto session = tutorial_session_state::snapshot();
    if (session.phase != tutorial_session_state::Phase::kInteractive) return false;

    const auto tutorial = tutorial_gameplay_state::snapshot();
    if (tutorial.step == tutorial_gameplay_state::Step::kJump &&
        control == Control::kButtonC &&
        gesture == Gesture::kTap) {
        return tutorial_gameplay_state::observe(
            tutorial_gameplay_state::Event::kJumpPerformed);
    }

    // Attack, Block, Counter-attack, Dodge and Air Attack intentionally do not
    // advance here. Their tutorial steps require gameplay outcomes rather than
    // a raw A/B/C input, so combat/player-state code must publish those events.
    return false;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = {};
}

bool observe(Control control, Gesture gesture) {
    if (!valid(control, gesture)) {
        std::lock_guard<std::mutex> lock(g_mutex);
        ++g_state.rejected_input_count;
        return false;
    }

    // Do not hold the action-input mutex while entering tutorial state: this
    // keeps the eventual Controls/combat lock order one-way.
    const bool tutorial_advanced = publish_tutorial_event(control, gesture);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.last_control = control;
    g_state.last_gesture = gesture;
    ++g_state.accepted_input_count;
    if (tutorial_advanced) ++g_state.tutorial_event_count;
    return true;
}

std::optional<int> original_controls_state(Control control) {
    switch (control) {
        case Control::kButtonA: return 7;
        case Control::kButtonB: return 8;
        case Control::kButtonC: return 9;
        case Control::kNone: return std::nullopt;
    }
    return std::nullopt;
}

std::optional<Control> control_for_original_controls_state(int state) {
    switch (state) {
        case 7: return Control::kButtonA;
        case 8: return Control::kButtonB;
        case 9: return Control::kButtonC;
        default: return std::nullopt;
    }
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* control_name(Control control) {
    switch (control) {
        case Control::kNone: return "none";
        case Control::kButtonA: return "button-a";
        case Control::kButtonB: return "button-b";
        case Control::kButtonC: return "button-c";
    }
    return "unknown";
}

const char* gesture_name(Gesture gesture) {
    switch (gesture) {
        case Gesture::kNone: return "none";
        case Gesture::kPress: return "press";
        case Gesture::kRelease: return "release";
        case Gesture::kTap: return "tap";
        case Gesture::kHoldBegin: return "hold-begin";
        case Gesture::kHoldEnd: return "hold-end";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameScene action input\n";
    out << "last control: " << control_name(state.last_control) << "\n";
    if (const auto original_state = original_controls_state(state.last_control)) {
        out << "last original Controls state: " << *original_state << "\n";
    } else {
        out << "last original Controls state: none\n";
    }
    out << "last gesture: " << gesture_name(state.last_gesture) << "\n";
    out << "accepted inputs: " << state.accepted_input_count << "\n";
    out << "rejected inputs: " << state.rejected_input_count << "\n";
    out << "tutorial events: " << state.tutorial_event_count << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_action_input
