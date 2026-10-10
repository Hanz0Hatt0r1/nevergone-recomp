#include <cassert>
#include <cstdint>
#include <optional>

#include "game_scene_direct_asset_frame_gate.h"

int main() {
    namespace gate = nevergone::game_scene_direct_asset_frame_gate;

    gate::State state;
    auto decision = gate::evaluate(std::nullopt, 0u, &state);
    assert(decision.action == gate::Action::kNone);

    decision = gate::evaluate(std::optional<std::uint64_t>(11u), 0u, &state);
    assert(decision.action == gate::Action::kStage);
    assert(decision.revision == 11u);

    // A failed staging attempt for the same revision is not retried every frame.
    decision = gate::evaluate(std::optional<std::uint64_t>(11u), 0u, &state);
    assert(decision.action == gate::Action::kNone);

    // Once the store publishes that revision, there is still nothing to do.
    decision = gate::evaluate(std::optional<std::uint64_t>(11u), 11u, &state);
    assert(decision.action == gate::Action::kNone);

    // A new scene/request revision is staged once.
    decision = gate::evaluate(std::optional<std::uint64_t>(12u), 11u, &state);
    assert(decision.action == gate::Action::kStage);
    assert(decision.revision == 12u);

    // Import reload explicitly allows retrying the same request revision.
    gate::invalidate(&state);
    decision = gate::evaluate(std::optional<std::uint64_t>(12u), 11u, &state);
    assert(decision.action == gate::Action::kStage);

    // Leaving the GameScene route clears stale active assets and resets retry state.
    decision = gate::evaluate(std::nullopt, 11u, &state);
    assert(decision.action == gate::Action::kClearStore);
    decision = gate::evaluate(std::optional<std::uint64_t>(12u), 0u, &state);
    assert(decision.action == gate::Action::kStage);

    // Revision zero is not a valid live request revision.
    gate::invalidate(&state);
    decision = gate::evaluate(std::optional<std::uint64_t>(0u), 0u, &state);
    assert(decision.action == gate::Action::kNone);
    return 0;
}
