#include "single_select_hero_rune_input.h"

#include <array>
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

struct RuneSourceSize {
    int width = 0;
    int height = 0;
};

std::mutex g_mutex;
int g_surface_width = 0;
int g_surface_height = 0;
std::array<RuneSourceSize, single_select_hero_rune_layout::kRuneCount> g_sizes{};
int g_pointer_id = -1;
int g_pressed_tag = 0;
bool g_pressed_inside = false;
std::uint64_t g_press_count = 0;
std::uint64_t g_selection_dispatch_count = 0;
std::uint64_t g_blocked_dispatch_count = 0;

void clear_press_locked() {
    g_pointer_id = -1;
    g_pressed_tag = 0;
    g_pressed_inside = false;
}

bool hit_locked(int tag, float x, float y) {
    if (!single_select_hero_rune_layout::valid_tag(tag)) return false;
    const RuneSourceSize size = g_sizes[static_cast<std::size_t>(tag - 1)];
    return single_select_hero_rune_layout::hit_test(
        size.width,
        size.height,
        tag,
        g_surface_width,
        g_surface_height,
        x,
        y);
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
    g_surface_width = 0;
    g_surface_height = 0;
    g_sizes = {};
    clear_press_locked();
    g_press_count = 0;
    g_selection_dispatch_count = 0;
    g_blocked_dispatch_count = 0;
}

void set_surface_size(int width, int height) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_surface_width = width > 0 ? width : 0;
    g_surface_height = height > 0 ? height : 0;
    if (g_surface_width == 0 || g_surface_height == 0) clear_press_locked();
}

void set_rune_source_size(int tag, int source_width, int source_height) {
    if (!single_select_hero_rune_layout::valid_tag(tag)) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    RuneSourceSize& size = g_sizes[static_cast<std::size_t>(tag - 1)];
    size.width = source_width > 0 ? source_width : 0;
    size.height = source_height > 0 ? source_height : 0;
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
                ++g_press_count;
                return true;
            }
            case kActionMove:
                if (g_pointer_id != pointer_id || g_pressed_tag == 0) return false;
                g_pressed_inside = hit_locked(g_pressed_tag, x, y);
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

    if (!captured || completed_tag == 0) return captured;

    const auto before = single_select_hero_state::snapshot();
    const bool accepted = single_select_hero_state::select_career(completed_tag);
    const auto after = single_select_hero_state::snapshot();

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (accepted) ++g_selection_dispatch_count;
        else ++g_blocked_dispatch_count;
    }

    if (accepted && after.generation == before.generation &&
            after.selection_count > before.selection_count &&
            after.selected_career != before.selected_career &&
            after.transition_pending) {
        (void)single_select_hero_transition_timeline::begin_career_change(
            game_clock::tick_count(),
            after.generation,
            before.selected_career,
            after.selected_career);
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
    out << "single select rune input: surface=" << g_surface_width << "x"
        << g_surface_height
        << " pointer=" << g_pointer_id
        << " pressed-tag=" << (g_pressed_inside ? g_pressed_tag : 0) << "\n";
    out << "single select rune input counts: press=" << g_press_count
        << " dispatch=" << g_selection_dispatch_count
        << " blocked=" << g_blocked_dispatch_count << "\n";
    return out.str();
}

}  // namespace nevergone::single_select_hero_rune_input
