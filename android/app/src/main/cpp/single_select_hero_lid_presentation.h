#pragma once

#include <cstdint>

#include "single_select_hero_coffin_motion.h"
#include "single_select_hero_transition_timeline.h"

namespace nevergone::single_select_hero_lid_presentation {

namespace motion = single_select_hero_coffin_motion;
namespace timeline = single_select_hero_transition_timeline;
namespace layout = single_select_hero_coffin_layout;

enum class Phase : int {
    kNone, kClosed, kClosingOld, kReel, kOpeningNew, kFinished,
    kUnknownStart
};

struct Sample {
    Phase phase = Phase::kNone;
    bool draw_lid = false;
    int lid_career = 0;
    float reel_x = 0.0f;
    float lid_local_x = 0.0f;
};

// An original CCSequence continues to play after FunOpenTheDoor has
// restored the input gate. The visual timeline is therefore NOT derived
// from Snapshot::phase or Snapshot::unlock_tick.
//
// We only display a cover when its initial pose is backed by the prior
// completed sequence. If a new selection interrupts an opening action,
// OpenTheDoor's numberOfRunningActions guard makes its starting position
// conditional: the renderer must abstain rather than invent that pose.
inline Sample sample(
        const timeline::VisualSnapshot& visual,
        std::uint64_t game_tick,
        std::uint64_t selector_generation) {
    Sample s;
    if (visual.phase == timeline::Phase::kInactive ||
            visual.selector_generation == 0 ||
            visual.selector_generation != selector_generation ||
            game_tick < visual.begin_tick) {
        return s;
    }
    if (!visual.initial_positions_verified) {
        s.phase = Phase::kUnknownStart;
        return s;
    }
    const float time = motion::seconds_from_ticks(game_tick, visual.begin_tick);
    if (visual.phase == timeline::Phase::kInitialCarousel) {
        const int career = static_cast<int>(visual.new_career);
        if (!motion::supported_career(career)) return s;
        const float first = motion::reel_total_seconds(career, career);
        s.lid_career = career;
        s.lid_local_x = layout::initial_lid_center_x(career - 1);
        if (time < first) {
            s.phase = Phase::kClosed;
            s.draw_lid = true;
            s.reel_x = motion::reel_x(career, career, time);
        } else if (time < first + motion::kOpenTotalSeconds) {
            s.phase = Phase::kOpeningNew;
            s.draw_lid = true;
            s.reel_x = layout::settled_reel_offset_x(career);
            s.lid_local_x = motion::opening_lid_x(
                s.lid_local_x, s.lid_local_x, time - first);
        } else {
            s.phase = Phase::kFinished;
        }
        return s;
    }

    if (visual.phase != timeline::Phase::kCareerChange ||
            !motion::supported_career(visual.old_career) ||
            !motion::supported_career(visual.new_career) ||
            visual.old_career == visual.new_career) return s;

    const int old_career = static_cast<int>(visual.old_career);
    const int new_career = static_cast<int>(visual.new_career);
    const float reel_duration =
        motion::reel_total_seconds(old_career, new_career);
    if (time < motion::kCloseTotalSeconds) {
        s.phase = Phase::kClosingOld;
        s.draw_lid = true;
        s.lid_career = old_career;
        s.reel_x = layout::settled_reel_offset_x(old_career);
        s.lid_local_x = motion::closing_lid_x(
            layout::initial_lid_center_x(old_career - 1), time);
    } else if (time < motion::kCloseTotalSeconds + reel_duration) {
        s.phase = Phase::kReel;
        s.draw_lid = true;
        s.lid_career = new_career;
        s.reel_x = motion::reel_x(
            old_career, new_career, time - motion::kCloseTotalSeconds);
        s.lid_local_x = layout::initial_lid_center_x(new_career - 1);
    } else if (time < motion::kCloseTotalSeconds +
                           reel_duration + motion::kOpenTotalSeconds) {
        s.phase = Phase::kOpeningNew;
        s.draw_lid = true;
        s.lid_career = new_career;
        s.reel_x = layout::settled_reel_offset_x(new_career);
        const float closed_x = layout::initial_lid_center_x(new_career - 1);
        s.lid_local_x = motion::opening_lid_x(
            closed_x, closed_x,
            time - motion::kCloseTotalSeconds - reel_duration);
    } else {
        s.phase = Phase::kFinished;
    }
    return s;
}

} // namespace nevergone::single_select_hero_lid_presentation
