# MEPlayer function-state baseline — B control correction — 2026-10-12

## Clean-room boundary

This note records bounded static behavior recovered from the shipped ARMv7 `libcocos2dcpp.so`. No original binary, disassembly listing, action data, or proprietary asset is committed.

## Controls to MEPlayer routing

`Controls::tick()` writes action-button function states into Controls `+0x118`: base A = `7`, base B = `8`, base C = `9`. `GameScene::tick()` passes Controls `+0x118` to `GameScene::setFunctionState()`, which forwards it to `MEPlayer::setFunctionState(CONTROLSSTATE)`.

This is distinct from Controls `+0x10c`, which is passed to `MEPlayer::setControlsState()` for movement/control state.

## Base actions

The recovered `MEPlayer::setFunctionState()` branches establish:

- function state `7` (A) -> `setActionState(0x02, false)`;
- function state `9` (C) -> `setActionState(0x20, false)`;
- function state `8` (B) is stateful and cannot be represented by one unconditional target.

For the current reconstructed player baseline, the player enters GameScene in MEPLAYERSTATE `0x1b`. With no prior guarded-action history in the relevant `+0x1f0/+0x1f8` fields, function state `8` falls through to `setActionState(0x1d, false)`. The same native B branch contains a separate guarded path to `setActionState(0x11, true)` for other live-state combinations.

Because MEPLAYERSTATE directly indexes the recovered action array, these correspond to:

| Controls function state | Baseline MEPLAYERSTATE | Action plist key |
| ---: | ---: | --- |
| `7` (A) | `0x02` | `action03` |
| `8` (B from initial `0x1b`) | `0x1d` | `action30` |
| `9` (C) | `0x20` | `action33` |

The guarded B alternative remains `0x11` / `action18` and is intentionally not generalized as the default.

## Project correction

`player_function_action_contract::base_action_request(8)` previously exposed the guarded `0x11,true` branch as if it were the base B request. That was too broad. It now exposes the observed current-project baseline `0x1d,false` while retaining `has_additional_player_guard=true` so runtime code does not treat B as globally unconditional.
