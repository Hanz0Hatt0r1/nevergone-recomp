#include <cassert>
#include <cstdint>
#include <vector>

#include "scene_actions_render_frame_plan.h"

int main() {
    nevergone::trap_actions_document::Document document;
    nevergone::enemy_actions_wbg_prefix::ActionFrameRecord frame;
    frame.first_string = "scene.plist";
    frame.third_string = "frame_01.png";
    frame.float_values[0] = 10.0f;
    frame.float_values[1] = 20.0f;
    frame.float_values[2] = 0.5f;
    frame.float_values[3] = 0.25f;
    frame.float_values[9] = 15.0f;
    frame.float_values[10] = 1.0f;
    frame.float_values[11] = 1.0f;
    document.core.primary.records.push_back(frame);

    nevergone::scene_action_initial_frame_pixel_store::Asset asset;
    asset.variant = 2;
    asset.slot = nevergone::scene_action_pair_document_store::Slot::kBorn;
    asset.mode_24e = 0u;
    asset.frame_index = 0u;
    asset.width = 2;
    asset.height = 2;
    asset.argb_pixels = {0xff000001u, 0xff000002u, 0xff000003u, 0xff000004u};
    asset.generation = 7u;

    nevergone::scene_actions_base_sprite_update::SystemInputs system;
    system.scale_240 = 1.0f;

    const auto plan = nevergone::scene_actions_render_frame_plan::build(
            document,
            asset,
            2,
            nevergone::scene_action_pair_document_store::Slot::kBorn,
            0u,
            system);
    assert(plan.has_value());
    assert(plan->frame_index == 0u);
    assert(plan->geometry.frame_index == 0u);
    assert(plan->geometry.atlas_plist == "scene.plist");
    assert(plan->geometry.frame_name == "frame_01.png");
    assert(plan->pixel_count == 4u);
    assert(plan->pixel_generation == 7u);

    auto stale = asset;
    stale.frame_index = 1u;
    assert(!nevergone::scene_actions_render_frame_plan::build(
            document,
            stale,
            2,
            nevergone::scene_action_pair_document_store::Slot::kBorn,
            0u,
            system).has_value());

    auto wrong_mode = asset;
    wrong_mode.mode_24e = 1u;
    assert(!nevergone::scene_actions_render_frame_plan::build(
            document,
            wrong_mode,
            2,
            nevergone::scene_action_pair_document_store::Slot::kBorn,
            0u,
            system).has_value());

    auto bad_pixels = asset;
    bad_pixels.argb_pixels.pop_back();
    assert(!nevergone::scene_actions_render_frame_plan::build(
            document,
            bad_pixels,
            2,
            nevergone::scene_action_pair_document_store::Slot::kBorn,
            0u,
            system).has_value());

    return 0;
}
