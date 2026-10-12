# Controls function state -> MEPlayer action — 2026-10-12

## Clean-room boundary

This note records bounded static behavior recovered from the shipped ARMv7 `libcocos2dcpp.so`. No proprietary binary, decoded assets, screenshots, heap dumps, or disassembly listings are committed.

## Runtime handoff

`GameScene::tick()` reads the active `Controls` object and forwards:

- `Controls + 0x110` to `GameScene::setDirComboState`;
- `Controls + 0x11c` to `GameScene::setControlsDirectionState`;
- `Controls + 0x118` to `GameScene::setFunctionState`;
- `Controls + 0x10c` to `GameScene::setControlsState`.

`GameScene::setFunctionState(CONTROLSSTATE)` resolves the local player through `MEPlayerManager` and calls `MEPlayer::setFunctionState` with the same value. This proves that `Controls + 0x118` is the action/function input boundary rather than a direct `MEPLAYERSTATE` field.

## Base A/B/C mapping

The shipped `MEPlayer::setFunctionState(CONTROLSSTATE)` contains these direct base-button requests:

| Controls function state | Source button | `MEPlayer::setActionState` request | Extra guard recovered here |
| ---: | --- | --- | --- |
| 7 | A | `(0x02, false)` | no button-specific guard beyond the function's general player-state gates |
| 8 | B | `(0x11, true)` | yes — checks current player fields at `+0x1f0`, `+0x1f8`, and action state `+0x278` |
| 9 | C | `(0x20, false)` | no button-specific guard beyond the function's general player-state gates |

The C path therefore supplies the concrete MEPlayer action-state request reached from the already captured tutorial instruction `Tap C to jump`: `MEPLAYERSTATE 0x20`.

## Why this is not executed blindly yet

`Controls::tick()` can synthesize function states beyond the base 7/8/9 values when buttons overlap with direction/combo state (for example the recovered A+C path reaches function state `0x1c`). `MEPlayer::setFunctionState` also rejects or redirects requests based on the current action/player state.

`player_function_action_contract` therefore exposes only the proven base request mapping. It deliberately does not call `setActionState` or assign animation frames. The next runtime layer must reconstruct the relevant MEPlayer gate fields and the additional synthesized Controls states before it can faithfully execute every A/B/C combat transition.
