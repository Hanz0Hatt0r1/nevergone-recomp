#include "management_role_selection_view.h"

namespace nevergone::management_role_selection_view {

ItemPose item_pose(std::size_t index, float visible_height, float item_height) {
    ItemPose pose;
    if (visible_height <= 0.0f || item_height <= 0.0f) return pose;
    pose.valid = true;
    pose.x = kItemCenterX;
    pose.y = visible_height - kFirstItemTopInset -
        static_cast<float>(index) * (item_height + kItemGap);
    return pose;
}

int hit_test(
        std::size_t item_count,
        float visible_height,
        float board_width,
        float board_height,
        float x,
        float y) {
    if (item_count == 0 || visible_height <= 0.0f ||
            board_width <= 0.0f || board_height <= 0.0f) {
        return -1;
    }

    const float half_width = board_width * 0.5f;
    const float half_height = board_height * 0.5f;
    for (std::size_t index = 0; index < item_count; ++index) {
        const ItemPose pose = item_pose(index, visible_height, board_height);
        if (!pose.valid) continue;
        if (x >= pose.x - half_width && x <= pose.x + half_width &&
                y >= pose.y - half_height && y <= pose.y + half_height) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

}  // namespace nevergone::management_role_selection_view
