#include <cassert>
#include <cmath>

#include "game_scene_type0_animation.h"

namespace {

bool near(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main() {
    namespace animation = nevergone::game_scene_type0_animation;
    namespace post = nevergone::game_scene_type0_post_init;

    post::RandomSkewLoopParams skew;
    skew.segment_duration_seconds = 2.0f;
    skew.skew_parameter = 3.0f;

    assert(near(animation::evaluate_random_skew_loop(skew, 0.0).skew_parameter, 0.0f));
    assert(near(animation::evaluate_random_skew_loop(skew, 1.0).skew_parameter, 1.5f));
    assert(near(animation::evaluate_random_skew_loop(skew, 2.0).skew_parameter, 3.0f));
    assert(near(animation::evaluate_random_skew_loop(skew, 3.0).skew_parameter, 0.0f));
    assert(near(animation::evaluate_random_skew_loop(skew, 4.0).skew_parameter, -3.0f));
    assert(near(animation::evaluate_random_skew_loop(skew, 5.0).skew_parameter, 0.0f));
    assert(near(animation::evaluate_random_skew_loop(skew, 6.0).skew_parameter, 3.0f));

    post::RandomFadeLoopParams fade;
    fade.delay_seconds = 6.0f;
    fade.fade_duration_seconds = 2.0f;
    fade.low_opacity = 25u;
    fade.high_opacity = 255u;

    assert(animation::evaluate_random_fade_loop(fade, 0.0).opacity == 255u);
    assert(animation::evaluate_random_fade_loop(fade, 5.0).opacity == 255u);
    assert(animation::evaluate_random_fade_loop(fade, 6.0).opacity == 255u);
    assert(animation::evaluate_random_fade_loop(fade, 7.0).opacity == 140u);
    assert(animation::evaluate_random_fade_loop(fade, 8.0).opacity == 25u);
    assert(animation::evaluate_random_fade_loop(fade, 9.0).opacity == 140u);
    assert(animation::evaluate_random_fade_loop(fade, 10.0).opacity == 255u);
    assert(animation::evaluate_random_fade_loop(fade, 16.0).opacity == 255u);

    post::RandomFadeLoopParams immediate;
    immediate.delay_seconds = 0.0f;
    immediate.fade_duration_seconds = 2.0f;
    immediate.low_opacity = 0u;
    immediate.high_opacity = 255u;
    assert(animation::evaluate_random_fade_loop(immediate, 1.0).opacity == 127u);
    assert(animation::evaluate_random_fade_loop(immediate, 2.0).opacity == 0u);
    assert(animation::evaluate_random_fade_loop(immediate, 3.0).opacity == 127u);
    assert(animation::evaluate_random_fade_loop(immediate, 4.0).opacity == 255u);

    return 0;
}
