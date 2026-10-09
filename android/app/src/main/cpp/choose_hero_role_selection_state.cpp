#include "choose_hero_role_selection_state.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <sstream>

namespace nevergone::choose_hero_role_selection_state {
namespace {

std::mutex g_mutex;
Snapshot g_state;

bool shipped_slot(std::uint32_t id) {
    return id == 1u || id == 2u;
}

const char* kind_name(ItemKind kind) {
    return kind == ItemKind::kCreateHero ? "create" : "hero";
}

}  // namespace

void reset(std::uint64_t scene_generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = {};
    g_state.scene_generation = scene_generation;
}

void configure_slots(
        const std::vector<std::uint32_t>& hero_slots,
        std::uint64_t scene_generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = {};
    g_state.scene_generation = scene_generation;

    bool have_slot_1 = false;
    bool have_slot_2 = false;
    for (const std::uint32_t id : hero_slots) {
        if (id == 1u) have_slot_1 = true;
        if (id == 2u) have_slot_2 = true;
    }

    if (have_slot_1) g_state.items.push_back({ItemKind::kHero, 1u, false});
    if (have_slot_2) g_state.items.push_back({ItemKind::kHero, 2u, false});

    const std::size_t hero_count = g_state.items.size();
    if (hero_count == 1u) {
        // initSaveDataUI adds createCreateHeroItem only for exactly one loaded
        // standalone hero. With two saves the original two-slot capacity is full.
        g_state.items.push_back({ItemKind::kCreateHero, 0u, false});
    }

    if (hero_count != 0u) {
        g_state.items.front().selected = true;
        g_state.current_hero_id = g_state.items.front().tag;
    }
}

bool select_tag(std::uint32_t tag) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto match = std::find_if(
        g_state.items.begin(),
        g_state.items.end(),
        [tag](const Item& item) { return item.tag == tag; });
    if (match == g_state.items.end()) return false;

    for (Item& item : g_state.items) item.selected = item.tag == tag;
    g_state.create_selected = match->kind == ItemKind::kCreateHero;
    if (match->kind == ItemKind::kHero && shipped_slot(match->tag)) {
        g_state.current_hero_id = match->tag;
    }
    ++g_state.selection_count;
    return true;
}

bool item_pose(
        std::size_t item_index,
        float visible_height,
        float item_height,
        ItemPose* output) {
    if (output == nullptr || !std::isfinite(visible_height) ||
            !std::isfinite(item_height) || visible_height <= 0.0f || item_height <= 0.0f) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (item_index >= g_state.items.size()) return false;
    output->x = 100.0f;
    output->y = visible_height - 100.0f -
        static_cast<float>(item_index) * (item_height + 15.0f);
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "ChooseHero role selection: items=" << state.items.size()
        << " current=" << state.current_hero_id
        << " create-selected=" << (state.create_selected ? "yes" : "no")
        << " generation=" << state.scene_generation << "\n";
    out << "ChooseHero role item tags:";
    for (const Item& item : state.items) {
        out << " " << kind_name(item.kind) << ":" << item.tag
            << (item.selected ? "*" : "");
    }
    out << "\n";
    return out.str();
}

}  // namespace nevergone::choose_hero_role_selection_state
