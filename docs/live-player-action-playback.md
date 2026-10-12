# Live player action playback

## Scope

The player render queue now consumes the complete currently loaded action document and the recovered `MeActionsSystem::waUpdate()` cursor instead of rebuilding a one-frame synthetic document on every queue request.

## Action lifecycle

`me_actions_live_playback` keys one playback instance by:

- `meplayer_live_action_state.generation`;
- committed `loaded_revision`;
- committed `current_action_state`.

When any of those changes, the cursor restarts with the recovered `MeActionsSystem::play()` initialization. This matches the point at which `PlayerActionRuntimeLoader` has successfully loaded the new imported `.actData` document and committed the desired MEPLAYERSTATE.

The state is advanced from `game_clock::tick_count()` using the existing fixed 35 Hz logical clock. A render queue may be queried more than once during one logical tick; repeated queries at the same tick are intentionally idempotent and cannot accelerate animation. If several logical ticks elapse between queries, their elapsed fixed-step time is accumulated in one `waUpdate()` call, which can itself advance several frames through the already recovered retained-accumulator loop.

A game-clock reset (for example GL surface recreation) moves only the tick baseline and cannot underflow elapsed time or jump the action cursor.

## Render queue

`game_scene_player_render_queue::current()` now:

1. fetches `player_action_document_store::current_document()`;
2. synchronizes/advances the live playback cursor when the committed live action is ready;
3. passes `current_frame_1a4` into `game_scene_player_frame_request::build()`;
4. resolves that frame's `ActionFrameData +0x68` sprite-frame name;
5. retains that selected frame's `ActionFrameData +0x60` atlas-plist identity for Android atlas staging.

Therefore tutorial jump (`0x20/action33`) and accepted A/B actions can request different imported sprite frames over time rather than remaining permanently on frame zero.

## Deliberate transform boundary

This increment changes the selected sprite frame but does **not** apply unproven later-frame `MeActionsSystem::updateData()` transform state. The body sprite continues to use the already recovered `play()`/frame-zero local transform while frame selection advances.

This is intentional. Later `updateData()` calls depend on additional mutable MeActions offsets and attachment/movement state. Applying the frame-zero simplification independently to every later frame would silently invent behavior. Per-frame position/anchor/scale/rotation reconstruction remains a separate evidence task.

## Completion

`me_actions_live_playback` retains the exact `done_220` transition produced by the recovered frame/boundary loop and counts completed actions. The completion signal is now available in native runtime state, but this increment does not yet synthesize `MEPlayer::setFunctionState(0)` return-to-idle behavior. That transition still requires the original completion gates to be connected explicitly.
