#include "single_select_hero_confirm_input.h"

#include <cmath>
#include <mutex>

#include "single_select_hero_confirm_layout.h"
#include "single_select_hero_state.h"

namespace nevergone::single_select_hero_confirm_input {
namespace {

std::mutex g_mutex;
int g_pointer_id = -1;
bool g_armed = false;
bool g_pressed = false;

void clear_locked() {
    g_pointer_id = -1;
    g_armed = false;
    g_pressed = false;
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

    if (surface_width <= 0 || surface_height <= 0 ||
            !std::isfinite(x) || !std::isfinite(y)) {
        reset();
        return true;
    }

    bool confirm = false;
    bool handled = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        switch (action) {
            case 0:
            case 5:
                if (g_pointer_id != -1) return true;
                if (!single_select_hero_confirm_layout::hit_test(
                        surface_width, surface_height, x, y)) {
                    return false;
                }
                g_pointer_id = pointer_id;
                g_armed = true;
                g_pressed = true;
                return true;

            case 2:
                if (g_pointer_id == -1 || !g_armed) return false;
                if (pointer_id != g_pointer_id) return true;
                g_pressed = single_select_hero_confirm_layout::hit_test(
                    surface_width, surface_height, x, y);
                return true;

            case 1:
            case 6:
                if (g_pointer_id == -1 || !g_armed) return false;
                if (pointer_id != g_pointer_id) return true;
                confirm = single_select_hero_confirm_layout::hit_test(
                    surface_width, surface_height, x, y);
                clear_locked();
                handled = true;
                break;

            case 3:
                if (g_pointer_id == -1) return false;
                clear_locked();
                return true;

            default:
                return g_pointer_id != -1;
        }
    }

    if (confirm) {
        // The recovered menuConfirm handler does not test the rune/carousel
        // interaction gate. Keep that behavior in single_select_hero_state and
        // do not add an invented input_enabled check here.
        (void)single_select_hero_state::confirm_online();
    }
    return handled;
}

bool pressed() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_pressed;
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear_locked();
}

}  // namespace nevergone::single_select_hero_confirm_input
