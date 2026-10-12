#include <cassert>

#include "scene_actions_current_frame_resource.h"

int main() {
    nevergone::trap_actions_document::Document document;
    nevergone::enemy_actions_wbg_prefix::ActionFrameRecord first;
    first.first_string = "scene_a.plist";
    first.third_string = "frame_00.png";
    nevergone::enemy_actions_wbg_prefix::ActionFrameRecord second;
    second.first_string = "scene_b.plist";
    second.third_string = "frame_01.png";
    document.core.primary.records.push_back(first);
    document.core.primary.records.push_back(second);

    const auto born = nevergone::scene_actions_current_frame_resource::build(document, 1u, 0u);
    assert(born.has_value());
    assert(born->frame_index == 1u);
    assert(born->atlas_plist == "scene_b.plist");
    assert(born->frame_name == "frame_01.png");
    assert(born->mode_24e == 0u);

    const auto idle = nevergone::scene_actions_current_frame_resource::build(document, 0u, 1u);
    assert(idle.has_value());
    assert(idle->mode_24e == 1u);

    assert(!nevergone::scene_actions_current_frame_resource::build(document, 2u, 0u).has_value());
    assert(!nevergone::scene_actions_current_frame_resource::build(document, 0u, 2u).has_value());

    document.core.primary.records[0].first_string.clear();
    assert(!nevergone::scene_actions_current_frame_resource::build(document, 0u, 0u).has_value());
    return 0;
}
