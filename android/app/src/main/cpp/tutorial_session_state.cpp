#include "tutorial_session_state.h"

#include <mutex>
#include <sstream>

#include "tutorial_gameplay_state.h"

namespace nevergone::tutorial_session_state {
namespace {

std::mutex g_mutex;
Snapshot g_state{};

bool advance_locked(Phase expected, Phase next) {
    if (g_state.phase != expected) {
        ++g_state.rejected_transition_count;
        return false;
    }
    g_state.phase = next;
    ++g_state.transition_count;
    return true;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = {};
    tutorial_gameplay_state::reset();
}

void begin_loading() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.generation;
    g_state.phase = Phase::kLoading;
    g_state.transition_count = 0;
    g_state.rejected_transition_count = 0;
    tutorial_gameplay_state::reset();
}

bool on_loading_complete() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return advance_locked(Phase::kLoading, Phase::kLessonOverlay);
}

bool on_lesson_overlay_complete() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!advance_locked(Phase::kLessonOverlay, Phase::kDialogue)) return false;
    tutorial_gameplay_state::begin_dialogue_sequence();
    return true;
}

bool on_dialogue_complete() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.phase != Phase::kDialogue) {
        ++g_state.rejected_transition_count;
        return false;
    }
    if (!tutorial_gameplay_state::observe(
            tutorial_gameplay_state::Event::kDialogueSequenceComplete)) {
        ++g_state.rejected_transition_count;
        return false;
    }
    g_state.phase = Phase::kInteractive;
    ++g_state.transition_count;
    return true;
}

bool on_interactive_complete() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.phase != Phase::kInteractive ||
        !tutorial_gameplay_state::snapshot().completed) {
        ++g_state.rejected_transition_count;
        return false;
    }
    g_state.phase = Phase::kComplete;
    ++g_state.transition_count;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* phase_name(Phase phase) {
    switch (phase) {
        case Phase::kInactive: return "inactive";
        case Phase::kLoading: return "loading";
        case Phase::kLessonOverlay: return "lesson-overlay";
        case Phase::kDialogue: return "dialogue";
        case Phase::kInteractive: return "interactive";
        case Phase::kComplete: return "complete";
    }
    return "unknown";
}

bool modal_input_blocked() {
    const Phase phase = snapshot().phase;
    return phase == Phase::kLoading ||
           phase == Phase::kLessonOverlay ||
           phase == Phase::kDialogue;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "tutorial session phase: " << phase_name(state.phase) << "\n";
    out << "tutorial session generation: " << state.generation << "\n";
    out << "tutorial session transitions: " << state.transition_count << "\n";
    out << "tutorial session rejected transitions: " << state.rejected_transition_count << "\n";
    out << "tutorial session modal input blocked: "
        << (modal_input_blocked() ? "yes" : "no") << "\n";
    return out.str();
}

}  // namespace nevergone::tutorial_session_state
