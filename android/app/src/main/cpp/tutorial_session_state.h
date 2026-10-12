#pragma once

#include <cstdint>
#include <string>

namespace nevergone::tutorial_session_state {

enum class Phase : int {
    kInactive = 0,
    kLoading = 1,
    kLessonOverlay = 2,
    kDialogue = 3,
    kInteractive = 4,
    kComplete = 5,
};

struct Snapshot {
    Phase phase = Phase::kInactive;
    std::uint64_t generation = 0;
    std::uint64_t transition_count = 0;
    std::uint64_t rejected_transition_count = 0;
};

void reset();
void begin_loading();
bool on_loading_complete();
bool on_lesson_overlay_complete();
bool on_dialogue_complete();
bool on_interactive_complete();

Snapshot snapshot();
const char* phase_name(Phase phase);
bool modal_input_blocked();
std::string status_report();

}  // namespace nevergone::tutorial_session_state
