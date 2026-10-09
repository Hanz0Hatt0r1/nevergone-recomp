#include "single_select_hero_pointer_router.h"

#include <array>
#include <atomic>

#include "character_name_state.h"
#include "game_clock.h"
#include "offline_startup_flow.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"
#include "single_select_hero_touch_state.h"
#include "single_select_hero_transition_timeline.h"

namespace nevergone::single_select_hero_pointer_router {
namespace {

constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionOutside = 4;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;

struct SourceSize {
    int width;
    int height;
};

// Recovered normal-rune content sizes from the shipped atlas. These are the
// CCMenuItemSprite touch sizes; TexturePacker visible trim does not shrink the
// menu item content rectangle.
constexpr std::array<SourceSize, single_select_hero_rune_layout::kRuneCount> kSourceSizes{{
    {79, 99},
    {87, 89},
    {75, 95},
    {111, 110},
    {137, 131},
}};

std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};

bool surface_active() {
    if (offline_startup_flow::snapshot().route !=
            offline_startup_flow::Route::kOpeningDialogue) {
        return false;
    }
    if (character_name_state::snapshot().active) return false;
    return single_select_hero_state::snapshot().active;
}

bool hit_tag(int tag, float x, float y) {
    if (!single_select_hero_rune_layout::valid_tag(tag)) return false;
    const auto& source = kSourceSizes[static_cast<std::size_t>(tag - 1)];
    return single_select_hero_rune_layout::hit_test(
        source.width,
        source.height,
        tag,
        g_surface_width.load(),
        g_surface_height.load(),
        x,
        y);
}

int hit_tag(float x, float y) {
    for (int tag = 1; tag <= single_select_hero_rune_layout::kRuneCount; ++tag) {
        if (hit_tag(tag, x, y)) return tag;
    }
    return 0;
}

}  // namespace

void set_surface_size(int width, int height) {
    g_surface_width.store(width > 0 ? width : 0);
    g_surface_height.store(height > 0 ? height : 0);
    if (width <= 0 || height <= 0) single_select_hero_touch_state::reset();
}

void reset() {
    single_select_hero_touch_state::reset();
}

bool on_touch(int action, int pointer_id, float surface_x, float surface_y) {
    const auto captured = single_select_hero_touch_state::snapshot();
    if (!surface_active()) {
        const bool owned = captured.pointer_id == pointer_id && captured.armed_tag != 0;
        single_select_hero_touch_state::reset();
        return owned;
    }

    if (action == kActionDown || action == kActionPointerDown) {
        const auto selector = single_select_hero_state::snapshot();
        if (!selector.input_enabled) return false;
        const int tag = hit_tag(surface_x, surface_y);
        return tag != 0 && single_select_hero_touch_state::begin(pointer_id, tag);
    }

    if (captured.pointer_id != pointer_id || captured.armed_tag == 0) return false;

    if (action == kActionMove) {
        (void)single_select_hero_touch_state::move(
            pointer_id, hit_tag(captured.armed_tag, surface_x, surface_y));
        return true;
    }

    if (action == kActionUp || action == kActionPointerUp) {
        int activated_tag = 0;
        const bool inside = hit_tag(captured.armed_tag, surface_x, surface_y);
        if (!single_select_hero_touch_state::release(
                pointer_id, inside, &activated_tag)) {
            return false;
        }
        if (activated_tag == 0) return true;

        const auto before = single_select_hero_state::snapshot();
        if (!before.active || !before.input_enabled ||
                character_name_state::snapshot().active) {
            return true;
        }
        if (!single_select_hero_state::select_career(activated_tag)) return true;

        const auto after = single_select_hero_state::snapshot();
        if (activated_tag != before.selected_career &&
                after.transition_pending && !after.input_enabled) {
            single_select_hero_transition_timeline::begin_career_change(
                game_clock::tick_count(), after.generation);
        }
        return true;
    }

    if (action == kActionCancel || action == kActionOutside) {
        (void)single_select_hero_touch_state::cancel(pointer_id);
        return true;
    }

    // Once a rune captured DOWN, retain ownership for uncommon MotionEvent
    // actions too so they cannot leak into TapToStart or another scene router.
    return true;
}

}  // namespace nevergone::single_select_hero_pointer_router
