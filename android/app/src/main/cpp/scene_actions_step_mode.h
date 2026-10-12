#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::scene_actions_step_mode {

enum class CompletionAction {
    kRemainDone = 0,
    kRestartAndStepAgain,
};

struct Decision {
    std::uint8_t mode_24e = 0;
    CompletionAction action = CompletionAction::kRemainDone;
    bool call_play = false;
    bool call_wa_step_again = false;
};

// SceneActionsSystem::step(float) checks isDone() after the normal waStep(dt).
// A completed mode-0 system returns as done. A completed non-zero mode system
// calls play() and immediately calls waStep(dt) again, making the recovered
// klhuo idle slot (mode 1) loop while the born slot (mode 0) remains complete.
// Only the two modes proven for the klhuo pair are accepted here.
inline std::optional<Decision> on_completed(std::uint8_t mode_24e) {
    if (mode_24e == 0u) {
        return Decision{0u, CompletionAction::kRemainDone, false, false};
    }
    if (mode_24e == 1u) {
        return Decision{1u, CompletionAction::kRestartAndStepAgain, true, true};
    }
    return std::nullopt;
}

}  // namespace nevergone::scene_actions_step_mode
