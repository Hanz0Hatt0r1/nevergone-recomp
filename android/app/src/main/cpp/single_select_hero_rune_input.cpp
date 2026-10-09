#include "single_select_hero_rune_input.h"

#include <cstdint>
#include <mutex>
#include <sstream>

#include "game_clock.h"
#include "offline_startup_flow.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_state.h"
#include "single_select_hero_transition_timeline.h"

namespace nevergone::single_select_hero_rune_input {
namespace {

constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;

std::mutex g_mutex;
int g_pointer_id = -1;
int g_pressed_tag = 0;
bool g_pressed_inside = false;
std::uint64_t g_press_count = 0;
std::uint64_t g_selection_dispatch_count = 0;
std::uint64_t g_blocked_dispatch_count = 0;

void publish_pressed_locked() {
    single_select_hero_rune_layout::set_runtime_pressed_tag(
        g_pressed_inside ? g_pressed_tag : 0);
}

void clear_press_locked() {
    g_pointer_id = -1;
    g_pressed_tag = 0;
    g_pressed_inside = false;
    publish_pressed_locked();
}

bool hit_locked(int tag, float x, float y) {
    return single_select_hero_rune_layout::hit_test_runtime(tag, x, y);
}

int hit_tag_locked(float x, float y) {
    for (int tag = 1; tag <= single_select_hero_rune_layout::kRuneCount; ++tag) {
        if (hit_locked(tag, x, y)) return tag;
    }
    return 0;
}

bool scene_accepts_rune_input() {
    return offline_startup_flow::snapshot().route ==
            offline_startup_flow::Route::kOpeningDialogue &&
        single_select_hero_state::snapshot().active;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear_press_locked();
    g_press_count = 0;
    g_selection_dispatch_count = 0;
    g_blocked_dispatch_count = 0;
    single_select_hero_rune_layout::reset_runtime_input_state();
}

bool on_touch(int action, int pointer_id, float x, float y) {
    if (!scene_accepts_rune_input()) {
        std::lock_guard<std::mutex> lock(g_mutex);
        clear_press_locked();
        return false;
    }

    int completed_tag = 0;
    bool captured = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        switch (action) {
            case kActionDown:
            case kActionPointerDown: {
                if (g_pointer_id >= 0) return false;
                const int tag = hit_tag_locked(x, y);
                if (tag == 0) return false;
                g_pointer_id = pointer_id;
                g_pressed_tag = tag;
                g_pressed_inside = true;
                publish_pressed_locked();
                ++g_press_count;
                return true;
            }
            case kActionMove:
                if (g_pointer_id != pointer_id || g_pressed_tag == 0) return false;
                g_pressed_inside = hit_locked(g_pressed_tag, x, y);
                publish_pressed_locked();
                return true;
            case kActionCancel:
                if (g_pointer_id < 0) return false;
                clear_press_locked();
                return true;
            case kActionUp:
            case kActionPointerUp:
                if (g_pointer_id != pointer_id || g_pressed_tag == 0) return false;
                captured = true;
                if (g_pressed_inside && hit_locked(g_pressed_tag, x, y)) {
                    completed_tag = g_pressed_tag;
                }
                clear_press_locked();
                break;
            default:
                return false;
        }
    }

    if (!captured) return false;
    if (completed_tag == 0) return true;

    const auto before = single_select_hero_state::snapshot();
    const bool accepted = single_select_hero_state::select_career(completed_tag);
    const auto after = single_select_hero_state::snapshot();

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (accepted) {
            ++g_selection_dispatch_count;
        } else {
            ++g_blocked_dispatch_count;
        }
    }

    if (accepted && after.selection_count > before.selection_count &&
            after.transition_pending && after.generation == before.generation) {
        single_select_hero_transition_timeline::begin_career_change(
            game_clock::tick_count(), after.generation);
    }
    return true;
}

int pressed_tag() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_pressed_inside ? g_pressed_tag : 0;
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream out;
    out << "single select rune input: surface="
        << single_select_hero_rune_layout::runtime_surface_width() << "x"
        << single_select_hero_rune_layout::runtime_surface_height()
        << " pointer=" << g_pointer_id
        << " pressed-tag=" << (g_pressed_inside ? g_pressed_tag : 0) << "\n";
    out << "single select rune input counts: press=" << g_press_count
        << " dispatch=" << g_selection_dispatch_count
        << " blocked=" << g_blocked_dispatch_count << "\n";
    return out.str();
}

}  // namespace nevergone::single_select_hero_rune_input
