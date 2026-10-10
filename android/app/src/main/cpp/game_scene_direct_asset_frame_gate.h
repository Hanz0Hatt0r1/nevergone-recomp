#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace nevergone::game_scene_direct_asset_frame_gate {

enum class Action {
    kNone = 0,
    kClearStore,
    kStage,
};

struct State {
    std::uint64_t last_attempt_revision = std::numeric_limits<std::uint64_t>::max();
};

struct Decision {
    Action action = Action::kNone;
    std::uint64_t revision = 0;
};

inline void invalidate(State* state) {
    if (state == nullptr) return;
    state->last_attempt_revision = std::numeric_limits<std::uint64_t>::max();
}

inline Decision evaluate(
        const std::optional<std::uint64_t>& live_revision,
        std::uint64_t active_revision,
        State* state) {
    if (state == nullptr) return {};
    if (!live_revision.has_value()) {
        invalidate(state);
        return {
            active_revision != 0 ? Action::kClearStore : Action::kNone,
            0,
        };
    }

    const std::uint64_t revision = *live_revision;
    if (revision == 0 || active_revision == revision ||
        state->last_attempt_revision == revision) {
        return {Action::kNone, revision};
    }

    state->last_attempt_revision = revision;
    return {Action::kStage, revision};
}

}  // namespace nevergone::game_scene_direct_asset_frame_gate
