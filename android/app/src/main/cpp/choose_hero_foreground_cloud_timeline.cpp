#include "choose_hero_foreground_cloud_timeline.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace nevergone::choose_hero_foreground_cloud_timeline {
namespace {

struct Spec {
    int frame_index;
    double delay_seconds;
    double move_seconds;
    float start_width_multiplier;
    float y_base;
    bool y_from_top;
    float opacity;
};

// Frame indices match ChooseHeroBackgroundComposer upload order:
// 7=qianjingyun01, 8=qianjingyun02, 9=qianjingyun03.
constexpr std::array<Spec, kInstanceCount> kSpecs{{
    // qianjingyun02: two copies at the top edge. The second repeats a
    // 20-second delay before every 40-second traverse.
    {8, 0.0, 40.0, 0.0f, 0.0f, true, 1.0f},
    {8, 20.0, 40.0, 0.0f, 0.0f, true, 1.0f},

    // qianjingyun03: lower band, opacity 178. The delayed copy is reset to
    // -2*width, exactly as the shipped CCMoveTo/CCDelayTime sequence does.
    {9, 0.0, 60.0, -1.0f, 100.0f, false, kDimOpacity},
    {9, 30.0, 60.0, -2.0f, 100.0f, false, kDimOpacity},

    // qianjingyun01: two vertically separated bands, opacity 178.
    {7, 0.0, 50.0, -1.0f, 350.0f, false, kDimOpacity},
    {7, 25.0, 50.0, -2.0f, 400.0f, false, kDimOpacity},
}};

float lerp(float a, float b, double t) {
    return a + (b - a) * static_cast<float>(t);
}

}  // namespace

bool pose_for_instance(
        std::size_t instance_index,
        double elapsed_seconds,
        float source_width,
        float source_height,
        Pose* output) {
    if (output == nullptr || instance_index >= kSpecs.size() ||
            !std::isfinite(elapsed_seconds) ||
            !std::isfinite(source_width) || !std::isfinite(source_height) ||
            source_width <= 0.0f || source_height <= 0.0f) {
        return false;
    }

    const Spec& spec = kSpecs[instance_index];
    const double clamped_elapsed = std::max(0.0, elapsed_seconds);
    const double period = spec.delay_seconds + spec.move_seconds;
    double phase = period > 0.0 ? std::fmod(clamped_elapsed, period) : 0.0;
    if (phase < 0.0) phase += period;

    const float start_x = spec.start_width_multiplier * source_width;
    const float end_x = kDesignWidth + source_width;
    float x = start_x;
    if (phase >= spec.delay_seconds && spec.move_seconds > 0.0) {
        const double move_phase = std::min(
            1.0,
            (phase - spec.delay_seconds) / spec.move_seconds);
        x = lerp(start_x, end_x, move_phase);
    }

    const float y = spec.y_from_top
        ? kDesignHeight - source_height * 0.5f
        : spec.y_base + source_height * 0.5f;

    output->frame_index = spec.frame_index;
    output->x = x;
    output->y = y;
    output->anchor_x = 1.0f;
    output->anchor_y = 0.5f;
    output->opacity = spec.opacity;
    return true;
}

}  // namespace nevergone::choose_hero_foreground_cloud_timeline
