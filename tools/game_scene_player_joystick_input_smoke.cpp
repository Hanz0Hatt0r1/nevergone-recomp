#include <cassert>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

#include "game_clock.h"
#include "game_scene_player_joystick_input.h"
#include "player_control_runtime_state.h"
#include "tutorial_gameplay_state.h"

namespace joystick = nevergone::game_scene_player_joystick_input;
namespace runtime = nevergone::player_control_runtime_state;
namespace contract = nevergone::player_control_contract;
namespace tutorial = nevergone::tutorial_gameplay_state;

namespace {

bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.001f;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

int main() {
    joystick::reset();
    assert(contains(joystick::status_report(), "captured: false"));
    assert(contains(runtime::status_report(), "primary state: none"));

    float design_x = 0.0f;
    float design_y = 0.0f;
    assert(joystick::surface_to_design(1136, 640, 204.0f, 467.0f, &design_x, &design_y));
    assert(near(design_x, joystick::kCenterX));
    assert(near(design_y, joystick::kCenterY));

    // ExactFit keeps the same design point under independent 2x scaling.
    assert(joystick::surface_to_design(2272, 1280, 408.0f, 934.0f, &design_x, &design_y));
    assert(near(design_x, joystick::kCenterX));
    assert(near(design_y, joystick::kCenterY));

    assert(joystick::on_touch(0, 7, 204.0f, 467.0f, 1136, 640));
    joystick::tick();
    auto state = runtime::snapshot();
    assert(!state.primary_state.has_value());
    assert(contains(joystick::status_report(), "captured: true"));
    assert(contains(joystick::status_report(), "accepted begins: 1"));

    // Positive local X -> angle 0 -> recovered primary state 0x13 -> no flip.
    assert(joystick::on_touch(2, 7, 304.0f, 467.0f, 1136, 640));
    joystick::tick();
    state = runtime::snapshot();
    assert(state.primary_state == contract::PrimaryControlState::kState13);
    assert(!state.flip_x);
    assert(contains(runtime::status_report(), "primary state: 0x13"));
    assert(contains(runtime::status_report(), "flip x: false"));

    // A different pointer cannot steal the captured fixed joystick.
    assert(!joystick::on_touch(2, 9, 104.0f, 467.0f, 1136, 640));

    // Negative local X -> +/-180 degrees -> state 0x12 -> flip-X enabled.
    assert(joystick::on_touch(2, 7, 104.0f, 467.0f, 1136, 640));
    joystick::tick();
    state = runtime::snapshot();
    assert(state.primary_state == contract::PrimaryControlState::kState12);
    assert(state.flip_x);
    assert(contains(runtime::status_report(), "primary state: 0x12"));
    assert(contains(runtime::status_report(), "flip x: true"));
    assert(contains(joystick::status_report(), "accepted moves: 2"));

    const auto before_far_move = joystick::snapshot();
    // CCJoyStick::updateTouchPoint rejects distance > (300 - 10) = 290 and
    // retains the preceding sample while the pointer remains captured.
    assert(joystick::on_touch(2, 7, 504.0f, 467.0f, 1136, 640));
    const auto after_far_move = joystick::snapshot();
    assert(after_far_move.captured);
    assert(near(after_far_move.power, before_far_move.power));
    assert(near(after_far_move.angle_degrees, before_far_move.angle_degrees));
    assert(after_far_move.accepted_moves == before_far_move.accepted_moves);

    assert(joystick::on_touch(1, 7, 104.0f, 467.0f, 1136, 640));
    assert(!joystick::snapshot().captured);
    joystick::tick();
    // Release neutralizes the joystick sample but retains visible facing.
    state = runtime::snapshot();
    assert(state.primary_state == contract::PrimaryControlState::kState12);
    assert(state.flip_x);
    assert(contains(joystick::status_report(), "captured: false"));
    assert(contains(runtime::status_report(), "primary state: 0x12"));

    // Native containsTouchLocation uses strict distance < hitRadius.
    assert(!joystick::on_touch(0, 8, 369.0f, 467.0f, 1136, 640));
    assert(joystick::on_touch(0, 8, 368.0f, 467.0f, 1136, 640));
    // ACTION_CANCEL terminates the entire Android gesture even when actionIndex
    // reports a different pointer from the one that originally captured it.
    assert(joystick::on_touch(3, 99, 900.0f, 100.0f, 1136, 640));
    assert(!joystick::snapshot().captured);

    // Scene/player teardown clears reconstructed facing as well as capture.
    assert(joystick::on_touch(0, 10, 204.0f, 467.0f, 1136, 640));
    assert(joystick::on_touch(2, 10, 104.0f, 467.0f, 1136, 640));
    joystick::tick();
    assert(runtime::snapshot().primary_state.has_value());
    joystick::reset();
    assert(!joystick::snapshot().captured);
    assert(!runtime::snapshot().primary_state.has_value());
    assert(contains(runtime::status_report(), "primary state: none"));

    // Invalid/out-of-surface coordinates fail closed.
    assert(!joystick::surface_to_design(0, 640, 1.0f, 1.0f, &design_x, &design_y));
    assert(!joystick::surface_to_design(1136, 640, -1.0f, 1.0f, &design_x, &design_y));

    // Integration: the same recovered fast-reactivation event that switches
    // movement into action state 1 also completes the original Run lesson.
    // The game clock clamps catch-up to <=5 ticks, so an 80 ms host sleep lands
    // deterministically inside the recovered 0.03..0.40 second reactivation
    // window even on a delayed CI runner.
    joystick::reset();
    nevergone::game_clock::reset();
    nevergone::game_clock::resume();
    tutorial::reset();
    tutorial::begin_dialogue_sequence();
    assert(tutorial::observe(tutorial::Event::kDialogueSequenceComplete));
    assert(tutorial::snapshot().step == tutorial::Step::kRun);

    assert(joystick::on_touch(0, 20, 304.0f, 467.0f, 1136, 640));
    joystick::tick();
    assert(joystick::on_touch(1, 20, 304.0f, 467.0f, 1136, 640));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    (void)nevergone::game_clock::advance();

    assert(joystick::on_touch(0, 21, 304.0f, 467.0f, 1136, 640));
    joystick::tick();
    assert(joystick::snapshot().fast_reactivation_count >= 1);
    assert(tutorial::snapshot().step == tutorial::Step::kJump);
    assert(tutorial::snapshot().last_event == tutorial::Event::kRunPerformed);

    return 0;
}
