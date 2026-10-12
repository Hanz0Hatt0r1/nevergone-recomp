#include "scene_actions_render_submit.h"

#include "scene_actions_gles_compositor.h"

namespace nevergone::scene_actions_render_submit {

Result submit(
        const scene_actions_active_playback::SlotState& state,
        const scene_actions_base_sprite_update::SystemInputs& system) {
    const auto prepared = scene_actions_render_runtime::prepare_from_store(state, system);
    if (prepared.status != scene_actions_render_runtime::PrepareStatus::kReady ||
            !prepared.frame.has_value()) {
        const bool retryable =
                prepared.status == scene_actions_render_runtime::PrepareStatus::kPixelsUnavailable ||
                prepared.status == scene_actions_render_runtime::PrepareStatus::kStalePixels;
        return {
            retryable ? SubmitStatus::kNotReady : SubmitStatus::kRejected,
            prepared.status,
        };
    }

    const auto& frame = *prepared.frame;
    if (!scene_actions_gles_compositor::submit(
                frame.plan,
                frame.asset,
                frame.parent_x,
                frame.parent_y,
                frame.child_z_order)) {
        return {SubmitStatus::kRejected, prepared.status};
    }
    return {SubmitStatus::kSubmitted, prepared.status};
}

}  // namespace nevergone::scene_actions_render_submit
