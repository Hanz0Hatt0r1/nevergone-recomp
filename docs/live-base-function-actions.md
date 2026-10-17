# Live base Controls function actions

## Scope

The reconstructed GameScene now has two independently recovered inputs to the live player-action document selector:

1. the existing tutorial semantic event for the first C/jump (`MEPLAYERSTATE 0x20`, `action33`);
2. the frame-sampled Controls function-state stream (`Controls +0x118`) produced by `controls_function_state_runtime`.

This change connects only action transitions whose live preconditions are currently supported without inventing original state.

## Accepted function actions

- Base A / Controls function state `7` calls `setActionState(0x02, false)`. The concrete action depends on the current state: walking (`0x00`) selects `0x10` (`action17`), running (`0x01`) selects `0x14` (`action21`), and the initial (`0x1b`) or adjacent (`0x1c`) state selects `0x02` (`action03`). Other current states remain fail-closed until their combo/interrupt gates are reconstructed.
- Base B / function state `8` selects `MEPLAYERSTATE 0x1d` (`action30`) **only** while the reconstructed player remains in the known initial `0x1b` baseline. The original B branch also depends on MEPlayer fields `+0x1f0/+0x1f8` and can select guarded `0x11` (`action18`); those live fields are not reconstructed yet, so B fails closed outside the initial baseline.
- Base C remains on the already-tested tutorial jump event path for this increment. The direct native C mapping remains `0x20` / `action33`.

The runtime consumes function-state **edges**, not every frame generation. Holding A or B therefore cannot continuously reload the same imported action document. The A request is selected using the live committed MEPlayer state; an unavailable state does not fall back to the initial attack.

## Release boundary

Controls returning `+0x118` to zero does not mean that MEPlayer immediately returns to initial action state `0x1b`. The original `MEPlayer::setFunctionState(0)` path consults current action state and action completion (`isActionDone()`). The recompilation uses the parsed action's recovered done flag. After walk/run attacks `0x10/0x14` finish, the native `actionState()` keeps that state; a later movement input may select walk, run, or idle.

For that reason a release edge is recorded but leaves the current/desired live action unchanged. Return-to-idle and repeated same-action animation restart remain explicit follow-up boundaries rather than timer guesses.

## Imported data

`PlayerActionRuntimeLoader` already resolves an arbitrary recovered MEPLAYERSTATE through `PlayerActionAssetResolver.resolveState()`. Therefore accepted A/B requests reuse the same user-imported action plist/actionsData path as the initial action and tutorial jump; no proprietary action bytes are added to the repository.
