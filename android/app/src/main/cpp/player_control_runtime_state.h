#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "player_control_contract.h"

namespace nevergone::player_control_runtime_state {

struct Snapshot {
    std::uint64_t generation = 0;
    std::optional<player_control_contract::PrimaryControlState> primary_state;
    bool flip_x = false;
};

// Drops reconstructed live-control state. The imported action/frame data remains
// untouched; a later valid input sample can establish facing again.
void reset();

// Applies one already-classified primary Controls state. Only evidence-backed
// horizontal states are accepted.
bool apply_primary_state(player_control_contract::PrimaryControlState state);

// Classifies one recovered Controls::onCCJoyStickUpdate sample and, when that
// sample reaches an evidence-backed horizontal assignment, applies it.
bool apply_horizontal_sample(float activity_metric, float angle_degrees);

Snapshot snapshot();

// Resource-name-free runtime diagnostics used by the retained Android evidence
// harness. The report exposes only reconstructed counters/state, never imported
// asset names or payloads.
std::string status_report();

}  // namespace nevergone::player_control_runtime_state
