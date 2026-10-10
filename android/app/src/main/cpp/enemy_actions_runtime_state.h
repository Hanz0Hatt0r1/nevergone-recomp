#pragma once

#include <cstddef>
#include <cstdint>

#include "enemy_actions_combo_consumer.h"
#include "enemy_actions_wbg_combo_section.h"

namespace nevergone::enemy_actions_runtime_state {

// Offset-named EnemyActionsSystem state composed only from fields already
// proven by comboHit()/waUpdate() evidence. Broader gameplay meanings remain
// intentionally unresolved.
struct State {
    std::uint8_t flag_169 = 0;
    std::uint8_t flag_190 = 0;
    std::uint8_t flag_191 = 0;
    std::uint8_t flag_1bc = 0;
    std::uint8_t flag_290 = 0;
    std::uint8_t flag_291 = 0;
    std::uint8_t flag_292 = 0;
    std::int32_t field_18c = 0;
    std::int32_t current_frame_294 = 0;
    std::size_t boundary_index_2a0 = 0;
    std::size_t boundary_index_2a4 = 0;
};

struct ComboHitResult {
    State state;
    bool returned_true = false;
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

// Composes only the proven waUpdate operations that occur after native timing
// logic has already produced current_frame_294. This function does NOT advance
// the frame, consume AFD+0x5c timing, call updateData(), or model animation.
WaUpdateAfterFrameResult apply_wa_update_after_frame_advance(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state);

}  // namespace nevergone::enemy_actions_runtime_state
