#include <cassert>

#include "scene_actions_step_mode.h"

namespace mode = nevergone::scene_actions_step_mode;

int main() {
    const auto born = mode::on_completed(0u);
    assert(born.has_value());
    assert(born->mode_24e == 0u);
    assert(born->action == mode::CompletionAction::kRemainDone);
    assert(!born->call_play);
    assert(!born->call_wa_step_again);

    const auto idle = mode::on_completed(1u);
    assert(idle.has_value());
    assert(idle->mode_24e == 1u);
    assert(idle->action == mode::CompletionAction::kRestartAndStepAgain);
    assert(idle->call_play);
    assert(idle->call_wa_step_again);

    assert(!mode::on_completed(2u).has_value());
    assert(!mode::on_completed(0xffu).has_value());
    return 0;
}
