# MEPlayer action completion transitions — 2026-10-12

## Clean-room boundary

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records bounded behavior from `MEPlayer::isActionDone()`, `MEPlayer::setActionState(MEPLAYERSTATE,bool)`, `MEPlayer::setFunctionState(CONTROLSSTATE)` and `MEPlayer::actionState()`. No original binary, disassembly listing, action payload, image or screenshot is committed.

## Completion source

`MEPlayer::isActionDone()` loads the current `MeActionsSystem` from MEPlayer offset `+0x234` and forwards directly to `MeActionsSystem::isDone()`. The latter is the already recovered byte at `MeActionsSystem +0x220`.

The live project cursor therefore has a concrete native completion source: `me_actions_live_playback.cursor.timing.done_220`.

## New action playback

The recovered tail of `MEPlayer::setActionState()` stops the previous `MeActionsSystem`, updates MEPlayer state `+0x278`, selects the corresponding action object from the retained action array, stores it at `+0x234`, calls `MeActionsSystem::play()` (or its special-parameter variant), and installs the MEPlayer play-done callback.

This matches the existing project boundary where `PlayerActionRuntimeLoader` resolves the requested MEPLAYERSTATE document and `commit_loaded_action()` marks the new action revision live.

## `MEPlayer::actionState()` transitions used here

When the current `MeActionsSystem` reports done, `MEPlayer::actionState()` dispatches on MEPlayer state `+0x278`.

The currently reconstructed controls require these exact branches:

- completed base A state `0x02` transitions to state `0x1b`;
- completed tutorial jump/C state `0x20` reaches the generic completion path and transitions to state `0x1b`;
- completed base B state `0x1d` reads current Controls/function state from MEPlayer `+0x1f4`:
  - if it remains `8`, transition to state `0x1e`;
  - otherwise transition to state `0x1b`.

For A/B the observed `setActionState` boolean argument is false. The generic jump completion path uses true. The current project action-document selection is keyed by MEPLAYERSTATE, so this increment preserves the proven target state while leaving the secondary boolean behavior to the later action-system parameter reconstruction where it materially affects runtime state.

## Why release alone is not enough

The neutral branch of `MEPlayer::setFunctionState(0)` calls `isActionDone()` but does not implement a universal `done -> 0x1b` rule. It has special handling around states `0x1d/0x1e`. Therefore button release is not used as a synthetic return-to-idle signal.

Instead, `meplayer_live_action_state::observe_action_completion()` consumes the concrete `done_220` signal and mirrors only the recovered `MEPlayer::actionState()` targets above.

## Stale/duplicate completion protection

A completion is accepted only when all of the following still identify the currently loaded action:

- action generation matches the live MEPlayer generation;
- loaded revision matches the committed document revision;
- completed state matches current MEPLAYERSTATE;
- current and desired states are still equal;
- loaded and desired revisions are still equal.

After the first accepted completion queues a target, current and desired differ. Repeated observations of the same persistent `done_220` therefore cannot create additional desired revisions.

## Runtime integration

`game_scene_player_render_queue::current()` already advances the current imported action document through `me_actions_live_playback`. When that cursor reaches `done_220`, it now publishes the completion together with the current reconstructed Controls function state.

The existing `PlayerActionRuntimeLoader` sees the new desired action on its next staging pass and reuses the normal resolve/load/commit pipeline. No special completion-only asset loader is introduced.
