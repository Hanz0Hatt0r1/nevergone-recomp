#pragma once

#include <cstddef>

namespace nevergone::management_role_selection_view {

constexpr float kItemCenterX = 100.0f;
constexpr float kFirstItemTopInset = 100.0f;
constexpr float kItemGap = 15.0f;

struct ItemPose {
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
};

// Uses the recovered ChooseHero item stack: X=100, first Y=visibleHeight-100,
// then subtract itemHeight+15 for each following role.
ItemPose item_pose(std::size_t index, float visible_height, float item_height);

// Hit-tests design-space coordinates against the role boards and returns the
// corresponding role-list index, or -1 when no item is hit.
int hit_test(
    std::size_t item_count,
    float visible_height,
    float board_width,
    float board_height,
    float x,
    float y);

}  // namespace nevergone::management_role_selection_view
