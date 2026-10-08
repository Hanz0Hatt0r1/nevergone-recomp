#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

#include "choose_hero_thunder_schedule.h"

namespace {

bool near(float left, float right, float epsilon = 0.00001f) {
    return std::fabs(left - right) <= epsilon;
}

}  // namespace

int main() {
    using namespace nevergone::choose_hero_thunder_schedule;

    assert(kInitialRandomBurnCount == 6);
    assert(near(unit_from_lrand48(0), 0.0f));
    assert(near(unit_from_lrand48(0x40000000u), 0.5f));

    const auto timing = menlei_timing(0x40000000u, 0x40000000u);
    assert(near(timing.delay_seconds, 5.5f));
    assert(near(timing.fade_seconds, 0.35f));

    const auto minimum = menlei_timing(0, 0);
    assert(near(minimum.delay_seconds, 3.0f));
    assert(near(minimum.fade_seconds, 0.0f));

    const auto indices = shandian_indices(
        0u,
        0x20000000u,  // u=0.25 -> floor(abs(.01-.25)*6)=1
        0x40000000u,  // u=0.5  -> floor(abs(.01-.5)*6)=2
        6);
    assert(indices[0] == 0);
    assert(indices[1] == 1);
    assert(indices[2] == 2);
    assert(indices[3] == 2);
    assert(indices[4] == 2);
    assert(indices[5] == 2);

    const auto empty_indices = shandian_indices(1u, 2u, 3u, 0);
    for (int index : empty_indices) assert(index == -1);

    assert(near(shandian_fade_seconds(0), 0.5f));
    assert(near(shandian_fade_seconds(0x40000000u), 0.65f));
    assert(near(ground_light_fade_seconds(0), 0.503f));
    assert(near(ground_light_fade_seconds(0x40000000u), 0.647f));

    // u≈0.01 maps to sound slot 0; u≈0.05 maps to slot 1; u=0.25 is silent.
    assert(thunder_sound_slot(21474836u) == 0);
    assert(thunder_sound_slot(107374182u) == 1);
    assert(thunder_sound_slot(0x20000000u) == -1);
    assert(thunder_sound_slot(kLrand48Max) == -1);

    assert(std::strcmp(
        thunder_sound_path(0),
        "SingleLogin_UI/L_thunder08-r.mp3") == 0);
    assert(std::strcmp(
        thunder_sound_path(6),
        "SingleLogin_UI/S_thunder_norm_1.mp3") == 0);
    assert(thunder_sound_path(-1) == nullptr);
    assert(thunder_sound_path(7) == nullptr);

    std::cout << "choose hero thunder schedule smoke: ok\n";
    return 0;
}
