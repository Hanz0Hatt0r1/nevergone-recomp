#pragma once

#include <string>

#include "player_control_runtime_state.h"
#include "tutorial_spawn_inputs.h"

namespace nevergone::tutorial_player_face_dir_provider {

struct Resolution {
    tutorial_spawn_inputs::ReviewedPlayerFaceDir face_dir{};
    bool control_consistent = false;

    bool ready() const {
        return control_consistent && face_dir.reviewed;
    }
};

// Original ARMv7 evidence ties PLAYERFACEDIR and the boolean facing contract to
// the same MEPlayer +0x12c field. The reconstructed primary controls path uses
// only states 0x12 and 0x13, which request true and false respectively.
// Resolve only when the retained flip_x agrees with that recovered state map.
Resolution resolve(const player_control_runtime_state::Snapshot& control);

std::string status_report(const Resolution& resolution);

}  // namespace nevergone::tutorial_player_face_dir_provider
