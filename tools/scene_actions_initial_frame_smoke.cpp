#include <cassert>

#include "scene_actions_initial_frame.h"

int main() {
    nevergone::trap_actions_document::Document document;
    nevergone::enemy_actions_wbg_prefix::ActionFrameRecord first;
    first.first_string = "scene/klhuo.plist";
    first.third_string = "klhuo_01.png";
    document.core.primary.records.push_back(first);

    const auto born = nevergone::scene_actions_initial_frame::build(document, 0u);
    assert(born.has_value());
    assert(born->primary_record_index == 0u);
    assert(born->atlas_plist == "scene/klhuo.plist");
    assert(born->frame_name == "klhuo_01.png");
    assert(born->mode_24e == 0u);

    const auto idle = nevergone::scene_actions_initial_frame::build(document, 1u);
    assert(idle.has_value());
    assert(idle->atlas_plist == born->atlas_plist);
    assert(idle->frame_name == born->frame_name);
    assert(idle->mode_24e == 1u);

    assert(!nevergone::scene_actions_initial_frame::build(document, 2u).has_value());

    nevergone::trap_actions_document::Document empty;
    assert(!nevergone::scene_actions_initial_frame::build(empty, 0u).has_value());

    document.core.primary.records.front().first_string.clear();
    assert(!nevergone::scene_actions_initial_frame::build(document, 0u).has_value());
    document.core.primary.records.front().first_string = "scene/klhuo.plist";
    document.core.primary.records.front().third_string.clear();
    assert(!nevergone::scene_actions_initial_frame::build(document, 0u).has_value());

    return 0;
}
