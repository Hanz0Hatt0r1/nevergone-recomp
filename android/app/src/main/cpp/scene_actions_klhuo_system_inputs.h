#pragma once

#include "scene_actions_base_sprite_update.h"

namespace nevergone::scene_actions_klhuo_system_inputs {

// The recovered klhuo GameSceneObject branch constructs both born and idle
// SceneActionsSystem instances with createWithFile(file, 0, 1.0f). Native
// initWithFile stores that float argument at +0x240 and initializes +0x244,
// +0x248 and flip byte +0x15e to zero before loading the document.
inline scene_actions_base_sprite_update::SystemInputs initial() {
    scene_actions_base_sprite_update::SystemInputs inputs;
    inputs.scale_240 = 1.0f;
    inputs.position_offset_x_244 = 0.0f;
    inputs.position_offset_y_248 = 0.0f;
    inputs.flip_x_15e = false;
    return inputs;
}

}  // namespace nevergone::scene_actions_klhuo_system_inputs
