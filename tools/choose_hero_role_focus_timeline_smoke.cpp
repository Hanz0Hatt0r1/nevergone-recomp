#include <cassert>
#include <cmath>

#include "choose_hero_role_focus_timeline.h"

namespace {

bool near(float a, float b, float epsilon = 0.002f) {
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main() {
    using namespace nevergone::choose_hero_role_focus_timeline;

    constexpr float board_width = 400.0f;
    constexpr float board_height = 100.0f;
    constexpr float highlight_width = 40.0f;
    constexpr float left = 120.0f;
    constexpr float right = 380.0f;
    constexpr float midpoint = 240.0f;

    HighlightPose top;
    HighlightPose bottom;
    assert(sample(0, 0.0, board_width, board_height, highlight_width, &top));
    assert(sample(1, 0.0, board_width, board_height, highlight_width, &bottom));
    assert(near(top.x, left));
    assert(near(top.y, 88.5f));
    assert(near(bottom.x, right));
    assert(near(bottom.y, 9.5f));
    assert(near(top.alpha, 0.0f));
    assert(near(bottom.alpha, 0.0f));

    // Delay(1) precedes the first FadeTo/MoveTo spawn.
    assert(sample(0, 1.0, board_width, board_height, highlight_width, &top));
    assert(near(top.x, left));
    assert(near(top.alpha, 0.0f));

    assert(sample(0, 1.625, board_width, board_height, highlight_width, &top));
    assert(sample(1, 1.625, board_width, board_height, highlight_width, &bottom));
    assert(near(top.x, (left + midpoint) * 0.5f));
    assert(near(bottom.x, (right + midpoint) * 0.5f));
    assert(near(top.alpha, kPeakAlpha * 0.5f));

    assert(sample(0, 2.25, board_width, board_height, highlight_width, &top));
    assert(near(top.x, midpoint));
    assert(near(top.alpha, kPeakAlpha));

    assert(sample(0, 2.875, board_width, board_height, highlight_width, &top));
    assert(sample(1, 2.875, board_width, board_height, highlight_width, &bottom));
    assert(near(top.x, (midpoint + right) * 0.5f));
    assert(near(bottom.x, (midpoint + left) * 0.5f));
    assert(near(top.alpha, kPeakAlpha * 0.5f));

    // At 3.5 seconds the two 1.25-second spawns are complete and alpha is 0.
    assert(sample(0, 3.5, board_width, board_height, highlight_width, &top));
    assert(sample(1, 3.5, board_width, board_height, highlight_width, &bottom));
    assert(near(top.x, right));
    assert(near(bottom.x, left));
    assert(near(top.alpha, 0.0f));

    // The shipped sequence resets position through a 0.01-second CCMoveTo.
    assert(sample(0, 3.505, board_width, board_height, highlight_width, &top));
    assert(near(top.x, (right + left) * 0.5f, 0.01f));
    assert(near(top.alpha, 0.0f));

    // Tail Delay(2) then CCRepeatForever starts the exact 5.51-second cycle again.
    assert(sample(0, kCycleSeconds, board_width, board_height, highlight_width, &top));
    assert(sample(1, kCycleSeconds, board_width, board_height, highlight_width, &bottom));
    assert(near(top.x, left));
    assert(near(bottom.x, right));
    assert(near(top.alpha, 0.0f));

    assert(!sample(2, 0.0, board_width, board_height, highlight_width, &top));
    assert(!sample(0, 0.0, 0.0f, board_height, highlight_width, &top));
    return 0;
}
