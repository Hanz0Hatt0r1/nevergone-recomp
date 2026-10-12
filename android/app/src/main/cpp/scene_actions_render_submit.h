#pragma once

#include "scene_actions_active_playback.h"
#include "scene_actions_base_sprite_update.h"
#include "scene_actions_render_runtime.h"

namespace nevergone::scene_actions_render_submit {

enum class SubmitStatus {
    kSubmitted = 0,
    kNotReady,
    kRejected,
};

struct Result {
    SubmitStatus status = SubmitStatus::kRejected;
    scene_actions_render_runtime::PrepareStatus prepare_status =
            scene_actions_render_runtime::PrepareStatus::kInactive;
};

Result submit(
        const scene_actions_active_playback::SlotState& state,
        const scene_actions_base_sprite_update::SystemInputs& system);

// Submit one klhuo frame using the recovered initial SceneActionsSystem values
// established by initWithFile/createWithFile. Later mutations remain explicit.
Result submit_initial_klhuo(const scene_actions_active_playback::SlotState& state);

}  // namespace nevergone::scene_actions_render_submit
