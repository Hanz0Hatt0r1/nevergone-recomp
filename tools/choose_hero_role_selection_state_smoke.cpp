#include <cassert>
#include <cmath>
#include <vector>

#include "choose_hero_role_selection_state.h"

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

}  // namespace

int main() {
    using namespace nevergone::choose_hero_role_selection_state;

    reset(4);
    auto state = snapshot();
    assert(state.scene_generation == 4);
    assert(state.items.empty());
    assert(state.current_hero_id == 0);

    // One shipped save => existing hero plus create tile. First hero is selected.
    configure_slots({1}, 5);
    state = snapshot();
    assert(state.scene_generation == 5);
    assert(state.items.size() == 2);
    assert(state.items[0].kind == ItemKind::kHero);
    assert(state.items[0].tag == 1);
    assert(state.items[0].selected);
    assert(state.items[1].kind == ItemKind::kCreateHero);
    assert(state.items[1].tag == 0);
    assert(!state.items[1].selected);
    assert(state.current_hero_id == 1);

    ItemPose pose;
    assert(item_pose(0, 640.0f, 120.0f, &pose));
    assert(near(pose.x, 100.0f));
    assert(near(pose.y, 540.0f));
    assert(item_pose(1, 640.0f, 120.0f, &pose));
    assert(near(pose.x, 100.0f));
    assert(near(pose.y, 405.0f));

    // Selecting create clears the visual hero selection but keeps the prior
    // preview/current hero id, matching updateHeroChooseButton.
    assert(select_tag(0));
    state = snapshot();
    assert(!state.items[0].selected);
    assert(state.items[1].selected);
    assert(state.create_selected);
    assert(state.current_hero_id == 1);
    assert(state.selection_count == 1);

    assert(select_tag(1));
    state = snapshot();
    assert(state.items[0].selected);
    assert(!state.items[1].selected);
    assert(!state.create_selected);
    assert(state.current_hero_id == 1);

    // Two shipped slots fill the original capacity; no create tile is added.
    configure_slots({2, 1, 2, 99}, 6);
    state = snapshot();
    assert(state.items.size() == 2);
    assert(state.items[0].tag == 1);
    assert(state.items[1].tag == 2);
    assert(state.items[0].selected);
    assert(state.current_hero_id == 1);
    assert(select_tag(2));
    state = snapshot();
    assert(!state.items[0].selected);
    assert(state.items[1].selected);
    assert(state.current_hero_id == 2);

    // Slot two by itself still gets the create item and becomes the default.
    configure_slots({2}, 7);
    state = snapshot();
    assert(state.items.size() == 2);
    assert(state.items[0].tag == 2);
    assert(state.items[0].selected);
    assert(state.items[1].kind == ItemKind::kCreateHero);
    assert(state.current_hero_id == 2);

    assert(!select_tag(99));
    assert(!item_pose(2, 640.0f, 120.0f, &pose));
    assert(!item_pose(0, 0.0f, 120.0f, &pose));
    return 0;
}
