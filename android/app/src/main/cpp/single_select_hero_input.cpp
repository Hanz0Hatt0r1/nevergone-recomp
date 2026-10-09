#include "single_select_hero_input.h"

#include <cmath>
#include <mutex>
#include <sstream>
#include <string>

#include "offline_startup_flow.h"
#include "render_bridge.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"

namespace nevergone::single_select_hero_input {
namespace {

std::mutex g_mutex;
int g_pointer_id = -1;
int g_armed_tag = 0;
int g_pressed_tag = 0;

bool route_active() {
    return offline_startup_flow::snapshot().route ==
            offline_startup_flow::Route::kOpeningDialogue &&
        single_select_hero_state::snapshot().active;
}

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

bool read_render_surface_size(int* width, int* height) {
    if (width == nullptr || height == nullptr) return false;
    const std::string report = render::status_report();
    static constexpr const char* kPrefix = "render surface: ";
    const std::size_t start = report.find(kPrefix);
    if (start == std::string::npos) return false;
    const std::size_t value_start = start + std::char_traits<char>::length(kPrefix);
    const std::size_t line_end = report.find('\n', value_start);
    const std::string value = report.substr(value_start, line_end - value_start);
    const std::size_t separator = value.find('x');
    if (separator == std::string::npos) return false;

    std::istringstream width_stream(value.substr(0, separator));
    std::istringstream height_stream(value.substr(separator + 1));
    int parsed_width = 0;
    int parsed_height = 0;
    width_stream >> parsed_width;
    height_stream >> parsed_height;
    if (!width_stream || !height_stream || parsed_width <= 0 || parsed_height <= 0) {
        return false;
    }
    *width = parsed_width;
    *height = parsed_height;
    return true;
}

}  // namespace

bool on_touch_for_surface(
        int action,
        int pointer_id,
        float x,
        float y,
        int surface_width,
        int surface_height) {
    if (!route_active()) {
        reset();
        return false;
    }

    // SingleSelectHero owns the whole opening-dialogue screen. Consume input
    // even while the recovered OpenTheDoor/Carousel gate is locked so events
    // never fall through into TapToStart or another mutually-exclusive layer.
    const auto state = single_select_hero_state::snapshot();
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

bool on_touch(int action, int pointer_id, float x, float y) {
    if (!route_active()) {
        reset();
        return false;
    }
    int width = 0;
    int height = 0;
    if (!read_render_surface_size(&width, &height)) {
        reset();
        return true;
    }
    return on_touch_for_surface(action, pointer_id, x, y, width, height);
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
