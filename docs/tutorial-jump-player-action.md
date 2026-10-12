# Tutorial C tap -> live player jump action

## Scope

This is the first bounded runtime transition from recovered Controls input into a different imported player action document. It is intentionally limited to the tutorial jump path; A/B combat transitions and general `MEPlayer::setActionState` remain separate reconstruction work.

## Evidence chain

The current clean-room chain is:

1. the recovered Controls C button is original function state `9`;
2. `MEPlayer::setFunctionState(9)` dispatches `setActionState(0x20, false)` after its common gates;
3. direct Thumb recovery of `MEPlayer::setActionState` shows current state `0x1b` accepts requested non-forced state `0x20` and stores it to MEPlayer `+0x278`;
4. `MEPlayer::setPlayerStateWithIdx` uses MEPLAYERSTATE directly as the zero-based `MeActionsSystem` array index;
5. `MEPlayer::createActions` fills that array from `action01`, `action02`, ... in order;
6. therefore state `0x20` resolves `action33` in the selected career's `pNN_actions.plist`.

The existing initial route is state `0x1b`, which resolves `action28`.

## Runtime behavior

`meplayer_live_action_state` is synchronized to the concrete reconstructed player-spawn generation. A fresh spawn begins with desired/current state `0x1b`.

`game_scene_action_input` increments `tutorial_event_count` only when the interactive C tap actually completes the recovered Jump lesson. The live action state uses that monotonic event rather than sampling whether C happens to be held during a rendered frame. This means a fast DOWN/UP pair cannot be missed merely because both events occur between two draws.

For a fresh player still in state `0x1b`, the next unseen tutorial event requests state `0x20`. The current action remains `0x1b` until the imported `action33` data resolves and parses successfully.

`PlayerActionRuntimeLoader` now asks native code for the desired action state every staging pass and resolves it with `PlayerActionAssetResolver.resolveState()`. Once the document has loaded, it commits the matching state to `meplayer_live_action_state`. The normal direct-asset staging pass then sees the new document's first player frame and creates a new texture-request revision in the same render-thread cycle.

## Deliberate limitations

- This step switches to the **first frame** of `action33`; it does not yet reproduce `MeActionsSystem` frame timing/playback.
- Only the proven tutorial transition `0x1b -> 0x20` is requested automatically.
- A/B are not executed yet, despite their recovered function-state and action mappings, because their general action-transition rules have not all been reduced into a live executor.
- If a desired imported action cannot be resolved or parsed, the loader records that action state as failed and does not repeat the expensive asset-tree scan every frame. Explicit imported-asset invalidation clears that sentinel and permits another attempt.
