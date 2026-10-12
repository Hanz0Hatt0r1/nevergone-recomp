#include <cassert>

#include "scene_actions_render_runtime.h"

namespace runtime = nevergone::scene_actions_render_runtime;
namespace docs = nevergone::scene_action_pair_document_store;

int main() {
    nevergone::trap_actions_document::Document document;
    nevergone::enemy_actions_wbg_prefix::ActionFrameRecord frame;
    frame.first_string = "scene.plist";
    frame.third_string = "frame_02.png";
    frame.float_values[0] = 10.0f;
    frame.float_values[1] = 20.0f;
    frame.float_values[2] = 0.5f;
    frame.float_values[3] = 0.25f;
    frame.float_values[9] = 15.0f;
    frame.float_values[10] = 1.0f;
    frame.float_values[11] = 1.0f;
    document.core.primary.records.push_back(frame);

    nevergone::scene_actions_active_playback::SlotState state;
    state.active = true;
    state.variant = 2;
    state.slot = docs::Slot::kBorn;
    state.parent_x = 44.0f;
    state.parent_y = 135.0f;
    state.child_z_order = 9;
    state.playback.mode_24e = 0u;
    state.playback.frame_index_250 = 0u;

    nevergone::scene_action_initial_frame_pixel_store::Asset asset;
    asset.variant = 2;
    asset.slot = docs::Slot::kBorn;
    asset.mode_24e = 0u;
    asset.frame_index = 0u;
    asset.width = 2;
    asset.height = 2;
    asset.argb_pixels = {1u, 2u, 3u, 4u};
    asset.generation = 11u;

    nevergone::scene_actions_base_sprite_update::SystemInputs system;
    system.scale_240 = 1.0f;

    auto result = runtime::prepare(state, &document, &asset, system);
    assert(result.status == runtime::PrepareStatus::kReady);
    assert(result.frame.has_value());
    assert(result.frame->plan.frame_index == 0u);
    assert(result.frame->plan.pixel_generation == 11u);
    assert(result.frame->parent_x == 44.0f);
    assert(result.frame->parent_y == 135.0f);
    assert(result.frame->child_z_order == 9);

    auto stale = asset;
    stale.frame_index = 1u;
    result = runtime::prepare(state, &document, &stale, system);
    assert(result.status == runtime::PrepareStatus::kStalePixels);
    assert(!result.frame.has_value());

    // Born terminal completion resets its cursor to zero without updateData.
    // A new render submission must therefore be rejected so the compositor can
    // keep the already-submitted final display frame.
    state.playback.done_1a8 = true;
    result = runtime::prepare(state, &document, &asset, system);
    assert(result.status == runtime::PrepareStatus::kCompleted);
    assert(!result.frame.has_value());
    state.playback.done_1a8 = false;

    state.active = false;
    result = runtime::prepare(state, &document, &asset, system);
    assert(result.status == runtime::PrepareStatus::kInactive);
    state.active = true;

    result = runtime::prepare(state, nullptr, &asset, system);
    assert(result.status == runtime::PrepareStatus::kDocumentUnavailable);
    result = runtime::prepare(state, &document, nullptr, system);
    assert(result.status == runtime::PrepareStatus::kPixelsUnavailable);

    auto bad = asset;
    bad.argb_pixels.pop_back();
    result = runtime::prepare(state, &document, &bad, system);
    assert(result.status == runtime::PrepareStatus::kInvalidFrame);
    return 0;
}
