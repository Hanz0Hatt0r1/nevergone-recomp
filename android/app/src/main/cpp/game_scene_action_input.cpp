#include "game_scene_action_input.h"

#include <mutex>
#include <sstream>

#include "tutorial_gameplay_state.h"
#include "tutorial_session_state.h"

namespace nevergone::game_scene_action_input {
namespace {

constexpr std::uint32_t kTutorialJumpMeplayerState = 0x20u;
constexpr int kQuickUseSlotCount = 2;

std::mutex g_mutex;
Snapshot g_state{};

bool valid(Control control, Gesture gesture) {
    return control != Control::kNone && gesture != Gesture::kNone;
}

bool is_tutorial_jump_request(Control control, Gesture gesture) {
    if (control != Control::kButtonC || gesture != Gesture::kTap) return false;

    const auto session = tutorial_session_state::snapshot();
    if (session.phase != tutorial_session_state::Phase::kInteractive) return false;

    return tutorial_gameplay_state::snapshot().step ==
        tutorial_gameplay_state::Step::kJump;
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

    // A C tap at the Jump lesson is only a request to enter the recovered live
    // MEPLAYERSTATE 0x20. Do not mutate tutorial progression here: the original
    // LessonLayer::nextLesson(MEPLAYERSTATE) checks the resulting state, not the
    // raw button gesture.
    const bool tutorial_jump_request = is_tutorial_jump_request(control, gesture);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.last_control = control;
    g_state.last_gesture = gesture;
    ++g_state.accepted_input_count;
    if (tutorial_jump_request) ++g_state.tutorial_jump_request_count;
    return true;
}

bool observe_quick_use_slot_tap(int slot) {
    if (slot < 0 || slot >= kQuickUseSlotCount) {
        std::lock_guard<std::mutex> lock(g_mutex);
        ++g_state.rejected_input_count;
        return false;
    }

    const auto session = tutorial_session_state::snapshot();
    const bool rage_item_tap = slot == 0 &&
        session.phase == tutorial_session_state::Phase::kInteractive &&
        tutorial_gameplay_state::snapshot().step ==
            tutorial_gameplay_state::Step::kFillRage;
    const bool rage_filled = rage_item_tap && tutorial_gameplay_state::observe(
        tutorial_gameplay_state::Event::kTutorialItemFilledRage);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.last_quick_use_slot = slot;
    ++g_state.accepted_input_count;
    ++g_state.quick_use_tap_count;
    if (rage_filled) ++g_state.tutorial_rage_fill_count;
    return true;
}

bool observe_live_meplayer_state(std::uint32_t meplayer_state) {
    if (meplayer_state != kTutorialJumpMeplayerState) return false;

    const auto session = tutorial_session_state::snapshot();
    if (session.phase != tutorial_session_state::Phase::kInteractive) return false;

    const auto tutorial = tutorial_gameplay_state::snapshot();
    if (tutorial.step != tutorial_gameplay_state::Step::kJump) return false;

    // Do not hold the action-input mutex while entering tutorial state. This
    // keeps the live-action/tutorial lock order one-way and makes repeated
    // observations of 0x20 idempotent after the first transition to Attack.
    const bool advanced = tutorial_gameplay_state::observe(
        tutorial_gameplay_state::Event::kJumpPerformed);
    if (!advanced) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.tutorial_event_count;
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
    out << "last quick-use slot: "
        << (state.last_quick_use_slot < 0
            ? "none"
            : std::to_string(state.last_quick_use_slot)) << "\n";
    out << "last gesture: " << gesture_name(state.last_gesture) << "\n";
    out << "accepted inputs: " << state.accepted_input_count << "\n";
    out << "rejected inputs: " << state.rejected_input_count << "\n";
    out << "quick-use taps: " << state.quick_use_tap_count << "\n";
    out << "tutorial Rage fills: " << state.tutorial_rage_fill_count << "\n";
    out << "tutorial jump requests: " << state.tutorial_jump_request_count << "\n";
    out << "tutorial events: " << state.tutorial_event_count << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_action_input
