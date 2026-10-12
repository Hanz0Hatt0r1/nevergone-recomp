#include "tutorial_gameplay_state.h"

#include <mutex>
#include <sstream>

namespace nevergone::tutorial_gameplay_state {
namespace {

std::mutex g_mutex;
Snapshot g_state{};

Step next_step_for(Step step, Event event) {
    switch (step) {
        case Step::kDialogue:
            return event == Event::kDialogueSequenceComplete ? Step::kRun : step;
        case Step::kRun:
            return event == Event::kRunPerformed ? Step::kJump : step;
        case Step::kJump:
            return event == Event::kJumpPerformed ? Step::kAttack : step;
        case Step::kAttack:
            return event == Event::kTutorialEnemiesDefeated ? Step::kBlock : step;
        case Step::kBlock:
            return event == Event::kSuccessfulBlock ? Step::kCounterAttack : step;
        case Step::kCounterAttack:
            return event == Event::kSuccessfulCounterAttack ? Step::kDodge : step;
        case Step::kDodge:
            return event == Event::kSuccessfulDodge ? Step::kAirAttack : step;
        case Step::kAirAttack:
            return event == Event::kTutorialBatDefeatedByAirAttack ? Step::kFillRage : step;
        case Step::kFillRage:
            return event == Event::kTutorialItemFilledRage ? Step::kExecution : step;
        case Step::kExecution:
            return event == Event::kSuccessfulExecution ? Step::kComplete : step;
        case Step::kInactive:
        case Step::kComplete:
            return step;
    }
    return step;
}

void refresh_flags_locked() {
    g_state.active = g_state.step != Step::kInactive && g_state.step != Step::kComplete;
    g_state.completed = g_state.step == Step::kComplete;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = {};
}

void begin_dialogue_sequence() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.generation;
    g_state.step = Step::kDialogue;
    g_state.last_event = Event::kNone;
    g_state.transition_count = 0;
    g_state.rejected_event_count = 0;
    refresh_flags_locked();
}

bool observe(Event event) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_state.active || event == Event::kNone) {
        ++g_state.rejected_event_count;
        return false;
    }

    const Step next = next_step_for(g_state.step, event);
    if (next == g_state.step) {
        ++g_state.rejected_event_count;
        return false;
    }

    g_state.last_event = event;
    g_state.step = next;
    ++g_state.transition_count;
    refresh_flags_locked();
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

Prompt prompt_for_step(Step step) {
    switch (step) {
        case Step::kRun:
            return {"Run", "Double-tap a direction to run.", Target::kJoystick};
        case Step::kJump:
            return {"Jump", "Tap C to jump.", Target::kButtonC};
        case Step::kAttack:
            return {
                "Attack",
                "Tap A to attack. Eliminate all the enemies in front of you.",
                Target::kButtonA,
            };
        case Step::kBlock:
            return {
                "Block",
                "Tap and hold B to [Block] the enemy attack.",
                Target::kButtonB,
            };
        case Step::kCounterAttack:
            return {
                "Counter-attack",
                "[Counter-attack] by attacking (tap A) immediately after a successful block.",
                Target::kButtonA,
            };
        case Step::kDodge:
            return {
                "Dodge",
                "Tap left or right while holding B to [Dodge] enemy attacks.",
                Target::kButtonB,
            };
        case Step::kAirAttack:
            return {
                "Air Attack",
                "Tap C to jump, then tap A to attack in the air. Use [Air Attack] to eliminate the bat.",
                Target::kButtonC,
            };
        case Step::kFillRage:
            return {
                "Execution",
                "Tap the potion in the item slot to max your rage so that you can unleash your [Execution].",
                Target::kItemSlot,
            };
        case Step::kExecution:
            return {
                "Execution",
                "Swipe from \"A\" to \"C\" to launch [Execution] when your [Rage] is full.",
                Target::kSwipeAtoC,
            };
        case Step::kInactive:
        case Step::kDialogue:
        case Step::kComplete:
            return {};
    }
    return {};
}

Prompt current_prompt() {
    return prompt_for_step(snapshot().step);
}

const char* step_name(Step step) {
    switch (step) {
        case Step::kInactive: return "inactive";
        case Step::kDialogue: return "dialogue";
        case Step::kRun: return "run";
        case Step::kJump: return "jump";
        case Step::kAttack: return "attack";
        case Step::kBlock: return "block";
        case Step::kCounterAttack: return "counter-attack";
        case Step::kDodge: return "dodge";
        case Step::kAirAttack: return "air-attack";
        case Step::kFillRage: return "fill-rage";
        case Step::kExecution: return "execution";
        case Step::kComplete: return "complete";
    }
    return "unknown";
}

const char* target_name(Target target) {
    switch (target) {
        case Target::kNone: return "none";
        case Target::kJoystick: return "joystick";
        case Target::kButtonA: return "button-a";
        case Target::kButtonB: return "button-b";
        case Target::kButtonC: return "button-c";
        case Target::kItemSlot: return "item-slot";
        case Target::kSwipeAtoC: return "swipe-a-to-c";
    }
    return "unknown";
}

const char* event_name(Event event) {
    switch (event) {
        case Event::kNone: return "none";
        case Event::kDialogueSequenceComplete: return "dialogue-sequence-complete";
        case Event::kRunPerformed: return "run-performed";
        case Event::kJumpPerformed: return "jump-performed";
        case Event::kTutorialEnemiesDefeated: return "tutorial-enemies-defeated";
        case Event::kSuccessfulBlock: return "successful-block";
        case Event::kSuccessfulCounterAttack: return "successful-counter-attack";
        case Event::kSuccessfulDodge: return "successful-dodge";
        case Event::kTutorialBatDefeatedByAirAttack: return "tutorial-bat-defeated-by-air-attack";
        case Event::kTutorialItemFilledRage: return "tutorial-item-filled-rage";
        case Event::kSuccessfulExecution: return "successful-execution";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    const Prompt prompt = prompt_for_step(state.step);
    std::ostringstream out;
    out << "tutorial gameplay step: " << step_name(state.step) << "\n";
    out << "tutorial gameplay active: " << (state.active ? "yes" : "no") << "\n";
    out << "tutorial gameplay completed: " << (state.completed ? "yes" : "no") << "\n";
    out << "tutorial gameplay generation: " << state.generation << "\n";
    out << "tutorial gameplay transitions: " << state.transition_count << "\n";
    out << "tutorial gameplay rejected events: " << state.rejected_event_count << "\n";
    out << "tutorial gameplay last event: " << event_name(state.last_event) << "\n";
    out << "tutorial gameplay target: " << target_name(prompt.target) << "\n";
    return out.str();
}

}  // namespace nevergone::tutorial_gameplay_state
