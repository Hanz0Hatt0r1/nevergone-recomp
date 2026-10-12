# Controls function-state synthesis — 2026-10-12

## Clean-room scope

This note records the action-button subset of the shipped ARMv7 `Controls::tick(float)` behavior. No binary or disassembly listing is committed.

`Controls + 0x118` is the function state forwarded each GameScene tick into `MEPlayer::setFunctionState`.

## Button priority

The recovered priority is:

1. A pressed + C pressed -> `0x1c`;
2. otherwise A pressed -> preserve a nonzero prior function state, or synthesize A state from the direction-combo inputs below;
3. otherwise B pressed -> base `8`, with one combo rewrite;
4. otherwise C pressed -> `9`;
5. no A/B/C pressed -> `0`.

## A rewrites

On first A activation (`previous +0x118 == 0`), base state is `7`.

- `+0x154 < 0.4` and direction combo `10` -> `0x14`;
- `+0x154 < 0.3` and direction combo `1` -> `0x16`;
- `+0x154 < 0.3` and direction combo `2` -> `0x15`;
- `+0x158 < 0.3` and direction combo `14` -> `0x17`;
- `+0x158 < 0.3` and direction combo `16` -> `0x18`.

The shipped first-A path then clears the direction-combo state and resets `+0x154` to zero.

## B rewrite

B writes base state `8`. When `+0x158 < 0.3` and direction combo state is `10`, it writes `0x1b` and clears the direction-combo state.

## Timer behavior

When `+0x154 > 0.5`, the shipped code resets that timer and clears direction-combo states `1`, `2`, `10`, and `11`.

After button synthesis:

- direction-combo `1`, `2`, `10`, or `11` advances `+0x154` by `delta`;
- direction-combo `14..19` advances `+0x158` by `delta`.

If the current player is unavailable, the original clears Controls function state (and Controls state) and returns before action timer advancement.

## Project contract

`controls_function_state_contract::step()` reproduces this bounded state transition as a pure function. It intentionally does not call `MEPlayer::setActionState`; execution still belongs behind the recovered MEPlayer current-state gates.
