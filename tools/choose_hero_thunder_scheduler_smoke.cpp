#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "choose_hero_thunder_scheduler.h"

namespace {

bool near(float left, float right, float epsilon = 0.0001f) {
    return std::fabs(left - right) <= epsilon;
}

}  // namespace

int main() {
    using namespace nevergone::choose_hero_thunder_scheduler;

    assert(near(normalize_lrand48(0u), 0.0f));
    assert(near(normalize_lrand48(0x40000000u), 0.5f));

    InitialRandomBatch initial;
    for (std::size_t index = 0; index < initial.discarded.size(); ++index) {
        initial.discarded[index] = static_cast<std::uint32_t>(index + 1);
    }
    initial.delay_raw = 0x40000000u;
    initial.fade_raw = 0x40000000u;
    const auto initial_result = initial_plan(initial);
    assert(near(initial_result.delay_seconds, 5.5f));
    assert(near(initial_result.fade_out_seconds, 0.35f));

    RescheduleRandomBatch reschedule;
    reschedule.lightning_index_raw_0 = 0u;
    reschedule.lightning_index_raw_1 = 0x40000000u;
    reschedule.lightning_index_raw_2 = 0x7fffffffu;
    reschedule.delay_raw = 0u;
    reschedule.thunder_fade_raw = 0x40000000u;
    const auto plan = reschedule_plan(reschedule, 6);
    assert(plan.lightning_indices[0] == 0);
    assert(plan.lightning_indices[1] == 2);
    assert(plan.lightning_indices[2] == 5);
    assert(plan.lightning_indices[3] == 5);
    assert(plan.lightning_indices[4] == 5);
    assert(plan.lightning_indices[5] == 5);
    assert(near(plan.thunder.delay_seconds, 3.0f));
    assert(near(plan.thunder.fade_out_seconds, 0.35f));

    // FuncThunderBen: abs(random-0.01)*30, truncated. Some random values
    // intentionally produce no effect rather than always selecting a sound.
    assert(thunder_sound_index(0u) == 0);
    assert(thunder_sound_index(0x0ccccccdu) == 2);  // about 0.1
    assert(thunder_sound_index(0x20000000u) == -1); // 0.25 -> index 7

    assert(near(lightning_fade_out_seconds(0u), 0.5f));
    assert(near(lightning_fade_out_seconds(0x40000000u), 0.65f));
    assert(near(ground_light_fade_out_seconds(0u), 0.503f));
    assert(near(ground_light_fade_out_seconds(0x40000000u), 0.647f));

    // Invalid lightning counts are represented as no selection rather than a
    // fabricated index.
    const auto empty = reschedule_plan(reschedule, 0);
    for (int index : empty.lightning_indices) assert(index == -1);

    std::cout << "choose hero thunder scheduler smoke: ok\n";
    return 0;
}
