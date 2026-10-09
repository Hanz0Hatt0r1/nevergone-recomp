#include "single_select_hero_input.h"

#include <cmath>
#include <mutex>
#include <utility>

#include "game_clock.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_timeline.h"

namespace nevergone::single_select_hero_input {
namespace {

struct RuneSize {
    int width = 0;
    int height = 0;
};

std::mutex g_mutex;
int g_pointer_id = -1;
int g_armed_tag = 0;
int g_pressed_tag = 0;

RuneSize source_size(int tag) {
    // Recovered untrimmed normal-frame source sizes from Singleselechero.plist.
    switch (tag) {
        case 1: return {79, 99};
        case 2: return {87, 89};
        case 3: return {75, 95};
        case 4: return {111, 110};
        case 5: return {137, 131};
        default: return {};
    }
}

void clear_gesture_locked() {
    g_pointer_id = -1;
    g_armed_tag = 0;
    g_pressed_tag = 0;
}

bool hit_tag(
        int tag,
        int surface_width,
        int surface_height,
        float x,
        float y) {
    const RuneSize size = source_size(tag);
    return single_select_hero_rune_layout::hit_test(
        size.width,
        size.height,
        tag,
        surface_width,
        surface_height,
        x,
        y);
}

int first_hit_tag(
        int surface_width,
        int surface_height,
        float x,
        float y) {
    for (int tag = 1; tag <= single_select_hero_rune_layout::kRuneCount; ++tag) {
        if (hit_tag(tag, surface_width, surface_height, x, y)) return tag;
    }
    return 0;
}

void commit_selection(int tag) {
    const auto before = single_select_hero_state::snapshot();
    if (!before.active || !before.input_enabled) return;
    if (!single_select_hero_state::select_career(tag)) return;

    const auto after = single_select_hero_state::snapshot();
    if (after.selected_career == before.selected_career ||
            !after.transition_pending || after.input_enabled) {
        return;
    }

    // menuOpenGC has now copied the sender tag and called OpenTheDoor(false).
    // Arm the already-recovered changed-career callback deadline from the same
    // fixed clock tick that accepted the native touch selection.
    single_select_hero_transition_timeline::begin_career_change(
        game_clock::tick_count(), after.generation);
}

}  // namespace

bool on_touch_for_surface(
        int action,
        int pointer_id,
        float x,
        float y,
        int surface_width,
        int surface_height) {
    const auto selector = single_select_hero_state::snapshot();
    if (!selector.active) {
        reset();
        return false;
    }

    // The fresh-account selector owns the full screen while active. During the
    // recovered OpenTheDoor/Carousel lock, consume events without changing the
    // selected career so they cannot leak into TapToStart or other routes.
    if (!selector.input_enabled || surface_width <= 0 || surface_height <= 0 ||
            !std::isfinite(x) || !std::isfinite(y)) {
        reset();
        return true;
    }

    int commit_tag = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        switch (action) {
            case 0:  // ACTION_DOWN
            case 5: {  // ACTION_POINTER_DOWN
                if (g_pointer_id != -1) return true;
                const int tag = first_hit_tag(surface_width, surface_height, x, y);
                if (tag != 0) {
                    g_pointer_id = pointer_id;
                    g_armed_tag = tag;
                    g_pressed_tag = tag;
                }
                return true;
            }
            case 2:  // ACTION_MOVE
                if (pointer_id == g_pointer_id && g_armed_tag != 0) {
                    g_pressed_tag = hit_tag(
                        g_armed_tag,
                        surface_width,
                        surface_height,
                        x,
                        y)
                        ? g_armed_tag
                        : 0;
                }
                return true;
            case 1:  // ACTION_UP
            case 6:  // ACTION_POINTER_UP
                if (pointer_id == g_pointer_id && g_armed_tag != 0) {
                    const int tag = g_armed_tag;
                    if (hit_tag(tag, surface_width, surface_height, x, y)) {
                        commit_tag = tag;
                    }
                    clear_gesture_locked();
                }
                break;
            case 3:  // ACTION_CANCEL
                clear_gesture_locked();
                return true;
            default:
                return true;
        }
    }

    if (commit_tag != 0) commit_selection(commit_tag);
    return true;
}

int pressed_tag() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_pressed_tag;
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear_gesture_locked();
}

}  // namespace nevergone::single_select_hero_input
