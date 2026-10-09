#include "single_select_hero_input.h"

#include <cmath>
#include <mutex>

#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"

namespace nevergone::single_select_hero_input {
namespace {

std::mutex g_mutex;
int g_pointer_id = -1;
int g_armed_tag = 0;
int g_pressed_tag = 0;

void clear_gesture_locked() {
    g_pointer_id = -1;
    g_armed_tag = 0;
    g_pressed_tag = 0;
}

int hit_tag(int surface_width, int surface_height, float x, float y) {
    for (int tag = 1; tag <= single_select_hero_rune_layout::kRuneCount; ++tag) {
        const auto geometry = single_select_hero_rune_layout::source_geometry_for_tag(tag);
        if (single_select_hero_rune_layout::hit_test(
                geometry, tag, surface_width, surface_height, x, y)) {
            return tag;
        }
    }
    return 0;
}

}  // namespace

bool on_touch_for_surface(
        int action,
        int pointer_id,
        float x,
        float y,
        int surface_width,
        int surface_height) {
    const auto state = single_select_hero_state::snapshot();
    if (!state.active) {
        reset();
        return false;
    }

    // SingleSelectHero owns the whole opening-dialogue screen. Consume input
    // even while the recovered OpenTheDoor/Carousel gate is locked so events
    // never fall through into TapToStart or another mutually-exclusive layer.
    if (!state.input_enabled || surface_width <= 0 || surface_height <= 0 ||
            !std::isfinite(x) || !std::isfinite(y)) {
        reset();
        return true;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    switch (action) {
        case 0:  // ACTION_DOWN
        case 5: {  // ACTION_POINTER_DOWN
            if (g_pointer_id != -1) return true;
            const int tag = hit_tag(surface_width, surface_height, x, y);
            if (tag != 0) {
                g_pointer_id = pointer_id;
                g_armed_tag = tag;
                g_pressed_tag = tag;
            }
            return true;
        }
        case 2: {  // ACTION_MOVE
            if (pointer_id != g_pointer_id || g_armed_tag == 0) return true;
            const auto geometry =
                single_select_hero_rune_layout::source_geometry_for_tag(g_armed_tag);
            g_pressed_tag = single_select_hero_rune_layout::hit_test(
                geometry,
                g_armed_tag,
                surface_width,
                surface_height,
                x,
                y)
                ? g_armed_tag
                : 0;
            return true;
        }
        case 1:  // ACTION_UP
        case 6: {  // ACTION_POINTER_UP
            if (pointer_id != g_pointer_id || g_armed_tag == 0) return true;
            const int tag = g_armed_tag;
            const auto geometry = single_select_hero_rune_layout::source_geometry_for_tag(tag);
            const bool inside = single_select_hero_rune_layout::hit_test(
                geometry, tag, surface_width, surface_height, x, y);
            clear_gesture_locked();
            if (inside && single_select_hero_state::snapshot().input_enabled) {
                (void)single_select_hero_state::select_career(tag);
            }
            return true;
        }
        case 3:  // ACTION_CANCEL
            clear_gesture_locked();
            return true;
        default:
            return true;
    }
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
