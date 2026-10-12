# Live base Controls function actions

## Scope

The reconstructed GameScene now has two independently recovered inputs to the live player-action document selector:

1. the existing tutorial semantic event for the first C/jump (`MEPLAYERSTATE 0x20`, `action33`);
2. the frame-sampled Controls function-state stream (`Controls +0x118`) produced by `controls_function_state_runtime`.

This change connects only action transitions whose live preconditions are currently supported without inventing original state.

## Accepted function actions

- Base A / Controls function state `7` selects `MEPLAYERSTATE 0x02` (`action03`). The recovered native `MEPlayer::setFunctionState()` branch is direct after its common action-state gates.
- Base B / function state `8` selects `MEPLAYERSTATE 0x1d` (`action30`) **only** while the reconstructed player remains in the known initial `0x1b` baseline. The original B branch also depends on MEPlayer fields `+0x1f0/+0x1f8` and can select guarded `0x11` (`action18`); those live fields are not reconstructed yet, so B fails closed outside the initial baseline.
- Base C remains on the already-tested tutorial jump event path for this increment. The direct native C mapping remains `0x20` / `action33`.

The runtime consumes function-state **edges**, not every frame generation. Holding A or B therefore cannot continuously reload the same imported action document.

## Release boundary

Controls returning `+0x118` to zero does not mean that MEPlayer immediately returns to initial action state `0x1b`. The original `MEPlayer::setFunctionState(0)` path consults current action state and action completion (`isActionDone()`). The current recompilation does not yet expose an equivalent completion signal from the parsed player action document.

For that reason a release edge is recorded but leaves the current/desired live action unchanged. Return-to-idle and repeated same-action animation restart remain explicit follow-up boundaries rather than timer guesses.

## Imported data

`PlayerActionRuntimeLoader` already resolves an arbitrary recovered MEPLAYERSTATE through `PlayerActionAssetResolver.resolveState()`. Therefore accepted A/B requests reuse the same user-imported action plist/actionsData path as the initial action and tutorial jump; no proprietary action bytes are added to the repository.
