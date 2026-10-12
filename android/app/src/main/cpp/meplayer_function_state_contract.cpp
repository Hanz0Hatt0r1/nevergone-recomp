#include "meplayer_function_state_contract.h"

#include <sstream>

namespace nevergone::meplayer_function_state_contract {

std::optional<Transition> unconditional_transition(int function_state) {
    switch (function_state) {
        case kButtonA:
            return Transition{kButtonABaseActionState, false};
        case kButtonC:
            return Transition{kButtonCJumpActionState, false};
        case kButtonB:
        case kNone:
        default:
            return std::nullopt;
    }
}

std::optional<Transition> baseline_transition_from_initial(int function_state) {
    if (const auto direct = unconditional_transition(function_state)) return direct;
    if (function_state == kButtonB) {
        return Transition{kButtonBFallbackActionState, false};
    }
    return std::nullopt;
}

bool is_recovered_action_button_state(int function_state) {
    return function_state == kButtonA ||
           function_state == kButtonB ||
           function_state == kButtonC;
}

std::string status_report() {
    std::ostringstream out;
    out << "MEPlayer function-state contract\n";
    out << "A: Controls 7 -> MEPLAYERSTATE 0x02 / action03\n";
    out << "B from initial 0x1b: Controls 8 -> MEPLAYERSTATE 0x1d / action30\n";
    out << "B guarded candidate: MEPLAYERSTATE 0x11 / action18\n";
    out << "C: Controls 9 -> MEPLAYERSTATE 0x20 / action33\n";
    return out.str();
}

}  // namespace nevergone::meplayer_function_state_contract
