#include <cassert>

#include "game_scene_action_input.h"
#include "game_scene_action_surface_input.h"
#include "tutorial_gameplay_state.h"
#include "tutorial_session_state.h"

namespace action = nevergone::game_scene_action_input;
namespace surface = nevergone::game_scene_action_surface_input;
namespace tutorial = nevergone::tutorial_gameplay_state;
namespace session = nevergone::tutorial_session_state;

namespace {

void enter_interactive_jump() {
    session::reset();
    tutorial::reset();
    action::reset();
    session::begin_loading();
    assert(session::on_loading_complete());
    assert(session::on_lesson_overlay_complete());
    assert(session::on_dialogue_complete());
    assert(tutorial::observe(tutorial::Event::kRunPerformed));
    assert(tutorial::snapshot().step == tutorial::Step::kJump);
}

}  // namespace

int main() {
    surface::reset();
    enter_interactive_jump();

    assert(surface::set_sprite_bounds(0, 100, 80));
    assert(surface::set_sprite_bounds(1, 100, 80));
    assert(surface::set_sprite_bounds(2, 100, 80));

    constexpr int kSurfaceWidth = 2272;
    constexpr int kSurfaceHeight = 1280;

    // C center is design (1000,224), Android top-origin surface (2000,832).
    assert(surface::on_touch(0, 7, 2000.0f, 832.0f, kSurfaceWidth, kSurfaceHeight));
    auto surface_state = surface::snapshot();
    assert(!surface_state.pressed[0]);
    assert(!surface_state.pressed[1]);
    assert(surface_state.pressed[2]);
    auto input = action::snapshot();
    assert(input.last_control == action::Control::kButtonC);
    assert(input.last_gesture == action::Gesture::kPress);
    assert(tutorial::snapshot().step == tutorial::Step::kJump);

    assert(surface::on_touch(1, 7, 2000.0f, 832.0f, kSurfaceWidth, kSurfaceHeight));
    surface_state = surface::snapshot();
    assert(!surface_state.pressed[2]);
    input = action::snapshot();
    assert(input.last_control == action::Control::kButtonC);
    assert(input.last_gesture == action::Gesture::kTap);
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
    assert(surface_state.taps == 1);

    // A begins at design center (898,90). Releasing outside its normal sprite
    // bounding box must release capture without manufacturing a tap.
    assert(surface::on_touch(0, 8, 1796.0f, 1100.0f, kSurfaceWidth, kSurfaceHeight));
    assert(surface::snapshot().pressed[0]);
    assert(surface::on_touch(1, 8, 1500.0f, 700.0f, kSurfaceWidth, kSurfaceHeight));
    assert(!surface::snapshot().pressed[0]);
    assert(surface::snapshot().taps == 1);
    input = action::snapshot();
    assert(input.last_control == action::Control::kButtonA);
    assert(input.last_gesture == action::Gesture::kRelease);

    assert(!surface::on_touch(0, 9, 300.0f, 900.0f, kSurfaceWidth, kSurfaceHeight));

    // Two right-side pointers can be captured independently and expose both
    // selected visuals. Cancellation clears every selected state.
    assert(surface::on_touch(0, 10, 1796.0f, 1100.0f, kSurfaceWidth, kSurfaceHeight));
    assert(surface::on_touch(5, 11, 2080.0f, 1100.0f, kSurfaceWidth, kSurfaceHeight));
    surface_state = surface::snapshot();
    assert(surface_state.captured_pointer_count == 2);
    assert(surface_state.pressed[0]);
    assert(surface_state.pressed[1]);
    surface::cancel_all_captures();
    surface_state = surface::snapshot();
    assert(surface_state.captured_pointer_count == 0);
    assert(surface_state.cancelled_captures == 2);
    assert(!surface_state.pressed[0]);
    assert(!surface_state.pressed[1]);
    assert(!surface_state.pressed[2]);

    return 0;
}
