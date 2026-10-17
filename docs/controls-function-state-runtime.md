# Live Controls function-state runtime

The project now has a live clean-room boundary between recovered on-screen Controls input and the already recovered `MEPlayer::setFunctionState` mapping.

## Inputs

Each GameLevels frame collects:

- the currently pressed A/B/C Controls buttons from `game_scene_action_surface_input`;
- the retained direction-combo state and its logical start tick from `game_scene_player_joystick_input`;
- whether the reconstructed player spawn is currently ready;
- the project fixed logical game tick.

The stateful core feeds those values through `controls_function_state_contract::step()`. It retains the previous function state and tracks when a specific direction-combo event has been consumed, so an event that the shipped Controls path cleared cannot reappear every frame merely because the joystick snapshot still retains the same state/start-tick pair.

## Output boundary

`controls_function_state_runtime::Snapshot` exposes the live recovered `Controls + 0x118` function state. For the three already proven base states it additionally exposes the corresponding pending `MEPlayer::setActionState` request:

- Controls state `7` / A -> MEPlayer state `0x10` from walk `0x00`, `0x14` from run `0x01`, or `0x02` from initial states `0x1b/0x1c`, all with `force=false`;
- Controls state `8` / B -> MEPlayer state `0x11`, `force=true`, with the still-unimplemented additional player guard;
- Controls state `9` / C -> MEPlayer state `0x20`, `force=false`.

The A action request is resolved from the committed live MEPlayer state; unknown or unsupported current states produce no request. Synthesized function states such as A+C `0x1c` or directional A rewrites remain visible as function state but deliberately produce no base action request until their `MEPlayer::setFunctionState` branches are recovered.

## Execution boundary

This runtime does **not** execute `MEPlayer::setActionState` yet. The original function has general current-player/action-state gates and the B path has additional guards. Applying an animation before those gates are recovered could make invalid attacks/jumps interrupt states that the shipped game rejects.

The next reconstruction step is therefore the MEPlayer gate contract. Once those gates are represented, this runtime already supplies the exact live function state and the evidence-backed base action request needed by that executor.
