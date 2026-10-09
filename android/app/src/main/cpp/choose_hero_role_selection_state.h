#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nevergone::choose_hero_role_selection_state {

enum class ItemKind : int {
    kHero = 0,
    kCreateHero = 1,
};

struct Item {
    ItemKind kind = ItemKind::kHero;
    std::uint32_t tag = 0;
    bool selected = false;
};

struct ItemPose {
    float x = 0.0f;
    float y = 0.0f;
};

struct Snapshot {
    std::uint64_t scene_generation = 0;
    std::vector<Item> items;
    std::uint32_t current_hero_id = 0;
    bool create_selected = false;
    std::uint64_t selection_count = 0;
};

void reset(std::uint64_t scene_generation = 0);

// Reconstructs ChooseHero::initSaveDataUI for the standalone loader's slots.
// Only shipped ids 1 and 2 are accepted; original loader order is preserved.
void configure_slots(
    const std::vector<std::uint32_t>& hero_slots,
    std::uint64_t scene_generation);

// Sender tags are the saved hero id for existing items and zero for the
// create-hero item. Selecting create keeps the prior preview hero id intact.
bool select_tag(std::uint32_t tag);

// Recovered item placement: X=100, first Y=visibleHeight-100 and subsequent
// items separated by itemHeight+15. Item height comes from the runtime board
// sprite/content size rather than a guessed constant.
bool item_pose(
    std::size_t item_index,
    float visible_height,
    float item_height,
    ItemPose* output);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::choose_hero_role_selection_state
