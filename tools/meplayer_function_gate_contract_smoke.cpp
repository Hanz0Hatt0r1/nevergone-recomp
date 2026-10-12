#include <cassert>

#include "meplayer_function_gate_contract.h"

namespace gate = nevergone::meplayer_function_gate_contract;

int main() {
    gate::Input in{};

    // Base A and C dispatch once the common gate is open.
    in.requested_function_state = 7u;
    auto out = gate::evaluate_base(in);
    assert(out.has_value());
    assert(out->kind == gate::OutcomeKind::kDispatch);
    assert(out->write_function_state && out->function_state_value == 7u);
    assert(out->action_state == 0x02u && !out->force);

    in.requested_function_state = 9u;
    out = gate::evaluate_base(in);
    assert(out.has_value());
    assert(out->kind == gate::OutcomeKind::kDispatch);
    assert(out->action_state == 0x20u && !out->force);

    // Unsupported synthesized states are deliberately outside this base gate.
    in.requested_function_state = 0x1cu;
    assert(!gate::evaluate_base(in).has_value());

    // Common current-action blocks happen before +0x1f4 is rewritten.
    for (std::uint32_t blocked : {0x42u, 0x39u, 0x3au, 0x3bu, 0x3cu, 0x3fu}) {
        in = {};
        in.requested_function_state = 9u;
        in.current_action_state = blocked;
        out = gate::evaluate_base(in);
        assert(out.has_value());
        assert(out->kind == gate::OutcomeKind::kReject);
        assert(!out->write_function_state);
    }

    // Repeated A/C are no-ops; B is the one repeated state that re-evaluates.
    in = {};
    in.requested_function_state = 7u;
    in.previous_function_state = 7u;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kReject);

    in = {};
    in.requested_function_state = 9u;
    in.previous_function_state = 9u;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kReject);

    // B is blocked outright while action 0x24 is active, even if +0x1f4 differs.
    in = {};
    in.requested_function_state = 8u;
    in.current_action_state = 0x24u;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kReject);
    assert(!out->write_function_state);

    // B special guard + action <= 9 -> forced state 0x11.
    in = {};
    in.requested_function_state = 8u;
    in.field_1f0 = 0x12u;
    in.current_action_state = 9u;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kDispatch);
    assert(out->write_function_state && out->function_state_value == 8u);
    assert(out->action_state == 0x11u && out->force);

    // The alternate +0x1f8 special values use the same branch.
    in = {};
    in.requested_function_state = 8u;
    in.field_1f8 = 0x0fu;
    in.current_action_state = 4u;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kDispatch);
    assert(out->action_state == 0x11u && out->force);

    // Special B guard with current action > 9 falls back to 0x1d unforced.
    in.current_action_state = 0x0au;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kDispatch);
    assert(out->action_state == 0x1du && !out->force);

    // Without the special B fields, action 0x0c latches +0x1f4 but performs no
    // setActionState call; all other current actions use the 0x1d fallback.
    in = {};
    in.requested_function_state = 8u;
    in.current_action_state = 0x0cu;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kNoAction);
    assert(out->write_function_state && out->function_state_value == 8u);

    in.current_action_state = 0x0bu;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kDispatch);
    assert(out->action_state == 0x1du && !out->force);

    // A repeated B still re-evaluates unless action 0x24 owns the player.
    in.previous_function_state = 8u;
    in.field_1f8 = 0x11u;
    in.current_action_state = 3u;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kDispatch);
    assert(out->action_state == 0x11u && out->force);

    // cp_lesson lock: mode 0xb, lesson 0xc and +0x328 >= 100 clears +0x1f4
    // and rejects every base A/B/C request before common dispatch.
    in = {};
    in.requested_function_state = 9u;
    in.previous_function_state = 7u;
    in.app_mode = 0x0bu;
    in.lesson_step = 0x0cu;
    in.field_328 = 100.0f;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kReject);
    assert(out->write_function_state && out->function_state_value == 0u);

    in.field_328 = 99.999f;
    out = gate::evaluate_base(in);
    assert(out->kind == gate::OutcomeKind::kDispatch);
    assert(out->action_state == 0x20u);

    return 0;
}
