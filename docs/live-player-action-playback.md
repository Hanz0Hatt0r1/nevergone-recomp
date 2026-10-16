# Live player action playback

## Scope

The player render queue consumes the complete currently loaded action document and the recovered `MeActionsSystem::waUpdate()` cursor instead of rebuilding a one-frame synthetic document on every queue request.

## Action lifecycle

`me_actions_live_playback` keys one playback instance by:

- `meplayer_live_action_state.generation`;
- committed `loaded_revision`;
- committed `current_action_state`.

When any of those changes, the cursor restarts with the recovered `MeActionsSystem::play()` initialization. This matches the point at which `PlayerActionRuntimeLoader` has successfully loaded the new imported `.actData` document and committed the desired MEPLAYERSTATE.

The state is advanced from `game_clock::tick_count()` using the existing fixed 35 Hz logical clock. A render queue may be queried more than once during one logical tick; repeated queries at the same tick are intentionally idempotent and cannot accelerate animation. If several logical ticks elapse between queries, their elapsed fixed-step time is accumulated in one `waUpdate()` call, which can itself advance several frames through the recovered retained-accumulator loop.

A game-clock reset (for example GL surface recreation) moves only the tick baseline and cannot underflow elapsed time or jump the action cursor.

## Render queue

`game_scene_player_render_queue::current()` now:

1. fetches `player_action_document_store::current_document()`;
2. synchronizes/advances the live playback cursor when the committed live action is ready;
3. passes `current_frame_1a4` into `game_scene_player_frame_request::build()`;
4. resolves that frame's `ActionFrameData +0x68` sprite-frame name;
5. retains that selected frame's `ActionFrameData +0x60` atlas-plist identity for Android atlas staging;
6. derives body position, anchor, scale, rotation, flip and child-Z from that same selected `ActionFrameData`.

Therefore tutorial jump (`0x20/action33`) and accepted A/B actions can change both imported sprite frame and its per-frame local transform over time rather than rendering all later frames with frame-zero geometry.

## Per-frame transform evidence

The shipped `MeActionsSystem::updateData()` body path resolves the current primary frame from `current_frame_1a4` and then applies the current frame's fields to the retained body sprite:

- `ActionFrameData +0x14/+0x18` -> local position inputs;
- `+0x24/+0x28` -> anchor;
- `+0x44/+0x48` -> scale inputs;
- `+0x34` -> final visible rotation;
- `+0x40` -> frame flip flag, combined with the system facing inversion;
- `+0x38` -> child Z;
- `+0x68` -> display-frame lookup.

For the project-supported `play(int,float)` path, native explicitly zeros `MeActionsSystem +0x204/+0x208` before the first `updateData()` call. The current clean-room runtime does not synthesize non-zero action-local position offsets, cut/H-point offsets, or alternate play-overload parameters. Within that bounded path the existing validated direct-frame transform can therefore be applied to every selected primary frame.

Facing remains overridden by the already reconstructed live `player_control_runtime_state`, matching the player-control route rather than freezing the initial frame's facing bit.

## Locomotion actions

The shipped `pNN_actions.plist` maps movement action state `0` to `action01`
(`pNN_w01.actData`, looping) and fast state `1` to `action02`
(`pNN_r01.actData`, looping). The recovered horizontal joystick path now queues
those states while walking or running and returns to the initial idle state
`0x1b` when the stick is released. The loader reuses its existing direct
state-to-action-data resolver, so each transition also selects the original
frame sequence and its matching atlas. Active jump/attack actions retain control
until their existing completion transition returns to idle.

## Remaining transform boundary

The renderer still does not synthesize unsupported mutable MeActions state such as non-zero action-local offsets, cut/H-point cropping, attachment transforms, ghosting, or alternate `play(float,float,float)` offset parameters. Those paths remain fail-closed/separate evidence work.

## Completion

`me_actions_live_playback` retains the exact `done_220` transition produced by the recovered frame/boundary loop and counts completed actions. `meplayer_live_action_state::observe_action_completion()` now consumes that concrete completion signal and mirrors the recovered supported `MEPlayer::actionState()` transitions, including the held-B follow-up state.
