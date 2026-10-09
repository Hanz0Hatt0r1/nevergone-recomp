#include "single_select_hero_confirm_input.h"

#include <atomic>
#include <cmath>
#include <mutex>

#include "single_select_hero_confirm_layout.h"
#include "single_select_hero_state.h"

namespace nevergone::single_select_hero_confirm_input {
namespace {

std::mutex g_mutex;
int g_pointer_id = -1;
bool g_armed = false;
std::atomic<bool> g_pressed{false};

void clear_locked() {
    g_pointer_id = -1;
    g_armed = false;
    g_pressed.store(false, std::memory_order_release);
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

    // The shipped CCMenuItemSprite is explicitly disabled while the recovered
    // OpenTheDoor/Carousel transition is locked. Preserve that external gate
    // even though menuConfirm() itself does not test the byte internally.
    if (!state.input_enabled || surface_width <= 0 || surface_height <= 0 ||
            !std::isfinite(x) || !std::isfinite(y)) {
        reset();
        return false;
    }

    bool commit = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        switch (action) {
            case 0:  // ACTION_DOWN
            case 5:  // ACTION_POINTER_DOWN
                if (g_pointer_id != -1) return g_armed;
                if (!single_select_hero_confirm_layout::hit_test(
                        surface_width, surface_height, x, y)) {
                    return false;
                }
                g_pointer_id = pointer_id;
                g_armed = true;
                g_pressed.store(true, std::memory_order_release);
                return true;

            case 2:  // ACTION_MOVE
                if (pointer_id != g_pointer_id || !g_armed) return false;
                g_pressed.store(
                    single_select_hero_confirm_layout::hit_test(
                        surface_width, surface_height, x, y),
                    std::memory_order_release);
                return true;

            case 1:  // ACTION_UP
            case 6:  // ACTION_POINTER_UP
                if (pointer_id != g_pointer_id || !g_armed) return false;
                commit = single_select_hero_confirm_layout::hit_test(
                    surface_width, surface_height, x, y);
                clear_locked();
                break;

            case 3:  // ACTION_CANCEL
                if (g_pointer_id == -1) return false;
                clear_locked();
                return true;

            default:
                return g_armed;
        }
    }

    if (commit) {
        // confirm_online() owns the proven existing-career equality block and
        // opens CharacterName state with selectedCareer unchanged on success.
        (void)single_select_hero_state::confirm_online();
    }
    return true;
}

bool pressed() {
    return g_pressed.load(std::memory_order_acquire);
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear_locked();
}

}  // namespace nevergone::single_select_hero_confirm_input
