#pragma once

#include <cstddef>
#include <cstdint>

#include "enemy_actions_combo_consumer.h"
#include "enemy_actions_wbg_combo_section.h"
#include "enemy_actions_wbg_final_table.h"

namespace nevergone::enemy_actions_runtime_state {

// Offset-named EnemyActionsSystem state composed only from fields already
// proven by comboHit()/waUpdate() evidence. Broader gameplay meanings remain
// intentionally unresolved.
struct State {
    std::uint8_t flag_168 = 0;
    std::uint8_t flag_169 = 0;
    std::uint8_t flag_190 = 0;
    std::uint8_t flag_191 = 0;
    std::uint8_t flag_1bc = 0;
    std::uint8_t flag_290 = 0;
    std::uint8_t flag_291 = 0;
    std::uint8_t flag_292 = 0;
    std::int32_t field_18c = 0;
    float field_158 = 0.0f;
    float field_15c = 0.0f;
    float field_160 = 0.0f;
    std::int32_t current_frame_294 = 0;
    std::size_t boundary_index_2a0 = 0;
    std::size_t boundary_index_2a4 = 0;
};

struct ComboHitResult {
    State state;
    bool returned_true = false;
};

struct WaUpdateTimingResult {
    State state;
    bool blocked_by_flag_1bc = false;
    bool had_valid_action_frame = false;
    float action_frame_5c = 0.0f;
    bool threshold_reached = false;
    bool frame_processing_allowed = false;
};

struct WaUpdateAfterFrameResult {
    State state;
    enemy_actions_combo_consumer::BoundaryCursorUpdate boundary_94;
    enemy_actions_combo_consumer::BoundaryCursorUpdate boundary_98;
    bool had_valid_boundary_94 = false;
    std::int32_t boundary_94_endpoint_18 = 0;
    bool reset_applied = false;
};

// Uses State+0x2a0 and State+0x294 as the recovered native selection/frame
// inputs, then applies the already-proven comboHit byte transition.
ComboHitResult apply_combo_hit(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state);

// Reconstructs the proven waUpdate() timing gate before the current frame is
// incremented. Section G's reciprocal_value is the exact reconstructed value
// stored at ActionFrameData+0x5c for each primary frame.
//
// Native sequence:
//   - nonzero +0x1bc exits immediately;
//   - nonzero +0x168 clears +0x168 and zeroes +0x160;
//   - otherwise +0x160 += delta_seconds * 1000.0f;
//   - +0x158 = ActionFrameData+0x5c + +0x15c;
//   - if +0x160 is below +0x158, exit;
//   - if +0x169 == 0, exit;
//   - otherwise +0x160 -= +0x158 and frame processing may continue.
// Invalid project-owned frame indices are a safe no-op after the accumulator
// update/reset step rather than reproducing invalid CCArray access.
WaUpdateTimingResult apply_wa_update_timing(
        const enemy_actions_wbg_final_table::Table& final_table,
        float delta_seconds,
        State state);

// Composes only the proven waUpdate operations that occur after native timing
// logic has already produced current_frame_294. This function does NOT advance
// the frame, consume AFD+0x5c timing, call updateData(), or model animation.
WaUpdateAfterFrameResult apply_wa_update_after_frame_advance(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state);

}  // namespace nevergone::enemy_actions_runtime_state
