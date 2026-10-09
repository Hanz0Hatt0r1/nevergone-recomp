#include "single_select_hero_touch_state.h"

#include <mutex>

#include "single_select_hero_rune_layout.h"

namespace nevergone::single_select_hero_touch_state {
namespace {
std::mutex g_mutex;
Snapshot g_state;
void clear_locked() { g_state = Snapshot{}; }
}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear_locked();
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

bool begin(int pointer_id, int tag) {
    if (pointer_id < 0 || !single_select_hero_rune_layout::valid_tag(tag)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.pointer_id >= 0) return false;
    g_state.pointer_id = pointer_id;
    g_state.armed_tag = tag;
    g_state.inside = true;
    return true;
}

bool move(int pointer_id, bool inside) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.pointer_id != pointer_id || g_state.armed_tag == 0) return false;
    g_state.inside = inside;
    return true;
}

bool release(int pointer_id, bool inside, int* activated_tag) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.pointer_id != pointer_id || g_state.armed_tag == 0) return false;
    if (activated_tag != nullptr) *activated_tag = inside ? g_state.armed_tag : 0;
    clear_locked();
    return true;
}

bool cancel(int pointer_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.pointer_id != pointer_id || g_state.armed_tag == 0) return false;
    clear_locked();
    return true;
}

}  // namespace nevergone::single_select_hero_touch_state
