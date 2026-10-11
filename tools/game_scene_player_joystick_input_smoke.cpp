#include <cassert>
#include <cmath>

#include "game_scene_player_joystick_input.h"
#include "player_control_runtime_state.h"

namespace joystick = nevergone::game_scene_player_joystick_input;
namespace runtime = nevergone::player_control_runtime_state;
namespace contract = nevergone::player_control_contract;

namespace {

bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.001f;
}

}  // namespace

int main() {
    runtime::reset();
    joystick::cancel_capture();

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

    // Positive local X -> angle 0 -> recovered primary state 0x13 -> no flip.
    assert(joystick::on_touch(2, 7, 304.0f, 467.0f, 1136, 640));
    joystick::tick();
    state = runtime::snapshot();
    assert(state.primary_state == contract::PrimaryControlState::kState13);
    assert(!state.flip_x);

    // A different pointer cannot steal the captured fixed joystick.
    assert(!joystick::on_touch(2, 9, 104.0f, 467.0f, 1136, 640));

    // Negative local X -> +/-180 degrees -> state 0x12 -> flip-X enabled.
    assert(joystick::on_touch(2, 7, 104.0f, 467.0f, 1136, 640));
    joystick::tick();
    state = runtime::snapshot();
    assert(state.primary_state == contract::PrimaryControlState::kState12);
    assert(state.flip_x);

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
    // Releasing neutralizes the joystick sample but retains the visible facing.
    state = runtime::snapshot();
    assert(state.primary_state == contract::PrimaryControlState::kState12);
    assert(state.flip_x);

    // Native containsTouchLocation uses strict distance < hitRadius.
    assert(!joystick::on_touch(0, 8, 369.0f, 467.0f, 1136, 640));
    assert(joystick::on_touch(0, 8, 368.0f, 467.0f, 1136, 640));
    assert(joystick::on_touch(3, 8, 368.0f, 467.0f, 1136, 640));

    // Invalid/out-of-surface coordinates fail closed.
    assert(!joystick::surface_to_design(0, 640, 1.0f, 1.0f, &design_x, &design_y));
    assert(!joystick::surface_to_design(1136, 640, -1.0f, 1.0f, &design_x, &design_y));
    return 0;
}
