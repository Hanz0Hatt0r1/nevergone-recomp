#pragma once

#include <optional>

#include "scene_action_pair_document_store.h"
#include "scene_actions_initial_frame.h"

namespace nevergone::scene_action_pair_initial_frame_runtime {

inline std::optional<scene_actions_initial_frame::Plan> current(
        scene_action_pair_document_store::Slot slot) {
    const auto document = scene_action_pair_document_store::current_document(slot);
    if (!document.has_value()) return std::nullopt;
    const std::uint8_t mode =
            slot == scene_action_pair_document_store::Slot::kBorn ? 0u : 1u;
    return scene_actions_initial_frame::build(*document, mode);
}

}  // namespace nevergone::scene_action_pair_initial_frame_runtime
