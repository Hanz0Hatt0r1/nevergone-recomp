#pragma once

#include <cstdint>
#include <string>

namespace nevergone::tutorial_gameplay_state {

enum class Step : int {
    kInactive = 0,
    kDialogue = 1,
    kRun = 2,
    kJump = 3,
    kAttack = 4,
    kBlock = 5,
    kCounterAttack = 6,
    kDodge = 7,
    kAirAttack = 8,
    kFillRage = 9,
    kExecution = 10,
    kComplete = 11,
};

enum class Target : int {
    kNone = 0,
    kJoystick = 1,
    kButtonA = 2,
    kButtonB = 3,
    kButtonC = 4,
    kItemSlot = 5,
    kSwipeAtoC = 6,
};

enum class Event : int {
    kNone = 0,
    kDialogueSequenceComplete = 1,
    kRunPerformed = 2,
    kJumpPerformed = 3,
    kTutorialEnemiesDefeated = 4,
    kSuccessfulBlock = 5,
    kSuccessfulCounterAttack = 6,
    kSuccessfulDodge = 7,
    kTutorialBatDefeatedByAirAttack = 8,
    kTutorialItemFilledRage = 9,
    kSuccessfulExecution = 10,
};

struct Prompt {
    const char* title = "";
    const char* instruction = "";
    Target target = Target::kNone;
};

struct Snapshot {
    Step step = Step::kInactive;
    Event last_event = Event::kNone;
    std::uint64_t generation = 0;
    std::uint64_t transition_count = 0;
    std::uint64_t rejected_event_count = 0;
    bool active = false;
    bool completed = false;
};

void reset();

// Enter the post-LessonLayer dialogue boundary observed on the tutorial scene.
// The dialogue renderer/driver owns the actual conversation pages; this state
// only records when that modal sequence has completed and gameplay tutorial
// input may start.
void begin_dialogue_sequence();

// Consume an evidence-backed tutorial completion event. Returns true only when
// the event is valid for the current step and advances the state machine.
bool observe(Event event);

Snapshot snapshot();
Prompt prompt_for_step(Step step);
Prompt current_prompt();

const char* step_name(Step step);
const char* target_name(Target target);
const char* event_name(Event event);
std::string status_report();

}  // namespace nevergone::tutorial_gameplay_state
