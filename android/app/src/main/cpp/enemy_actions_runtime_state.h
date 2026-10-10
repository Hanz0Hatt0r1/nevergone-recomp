#pragma once

#include <cstddef>
#include <cstdint>

#include "enemy_actions_combo_consumer.h"
#include "enemy_actions_wbg_combo_section.h"
#include "enemy_actions_wbg_document.h"
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
    std::int32_t previous_frame_154 = 0;
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

struct WaUpdateFrameStepResult {
    State state;
    enemy_actions_combo_consumer::BoundaryCursorUpdate boundary_94;
    enemy_actions_combo_consumer::BoundaryCursorUpdate boundary_98;
    bool had_valid_boundary_94 = false;
    bool used_no_boundary_94_fallback = false;
    bool reset_applied = false;
    bool previous_frame_changed = false;
    bool should_call_update_data = false;
};

struct WaUpdateIterationResult {
    State state;
    WaUpdateTimingResult timing;
    WaUpdateFrameStepResult frame_step;
    bool frame_step_applied = false;
};

ComboHitResult apply_combo_hit(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state);

WaUpdateTimingResult apply_wa_update_timing(
        const enemy_actions_wbg_final_table::Table& final_table,
        float delta_seconds,
        State state);

WaUpdateAfterFrameResult apply_wa_update_after_frame_advance(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        State state);

WaUpdateFrameStepResult apply_wa_update_frame_step(
        const enemy_actions_wbg_combo_section::Block& combo_block,
        std::uint32_t action_frame_count,
        State state);

// Composes exactly one recovered waUpdate iteration using a parsed Sections A-G
// document: timing gate first, then one frame-processing step only when the
// native gate allows it. This intentionally does not reproduce the native
// back-edge that may consume additional accumulated frames in the same call.
WaUpdateIterationResult apply_wa_update_iteration(
        const enemy_actions_wbg_document::Document& document,
        float delta_seconds,
        State state);

}  // namespace nevergone::enemy_actions_runtime_state
