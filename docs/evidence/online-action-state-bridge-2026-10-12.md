# Online action-state bridge — 2026-10-12

## Scope

This note continues the recovered PVE path from `GameScene::olUpdataPlayer2State` / `GameScene::olUpdataEnemyState` into the existing player/enemy action systems.

Original NeverGone 1.0.9 ARMv7 `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

Relevant native functions:

- `MEPlayer::setPlayerStateWithIdx(MEPLAYERSTATE, unsigned int)` — `0x0031ce0a`
- `MEPlayer::setTeamActionState(MEPLAYERSTATE, bool)` — `0x0031ddb0`
- `EnemyObject::onlineEnemyState(ENEMYSTATE, bool)` — `0x002b0584`
- `EnemyObject::setOlEnemyHealth(int, int)` — `0x002b0870`

## `MEPlayer::setTeamActionState`

This small wrapper first calls `MEPlayer::isSelfMoveState(state)` and compares the requested state with the current state at `MEPlayer + 0x278`.

Recovered behavior:

```text
if (requested_state != current_state) {
    setActionState(requested_state, bool_arg);
} else if (isSelfMoveState(requested_state)) {
    setPlayerPos(position_at_MEPlayer_plus_0x248);
}
```

A repeated non-movement state does not restart the action. A repeated self-movement state re-applies the tracked position instead. This explains why the ordinary online-player path can call `setTeamActionState(os, true)` on every received state packet without necessarily restarting animation playback every packet.

## `MEPlayer::setPlayerStateWithIdx`

The function stores the requested `MEPLAYERSTATE` at `MEPlayer + 0x278`, except state `0x43`, which immediately returns through the function epilogue without entering the normal selection/playback path.

It stops the currently selected `MeActionsSystem`, selects the action object associated with the requested state, attaches/positions it, and then chooses one of two playback forms.

### Indexed playback is restricted to states `0x20` and `0x21`

At `0x0031cf00..0x0031cf22` native computes `state - 0x20` and accepts only values `0` or `1`.

If the state is `0x20` or `0x21` **and** the supplied index is nonzero:

```text
MeActionsSystem::play(index, saved_frame_value)
MEPlayerSound::setPlayedFlag()
```

Otherwise:

```text
MeActionsSystem::play()
```

The float `saved_frame_value` is loaded from the previous/current action-system field at `MeActionsSystem + 0x1c8` before the old action is stopped.

Therefore the PVE `playerActionDataMsg.sGotoFrameIdx` value becomes an actual indexed action playback argument only for remote-player states `0x20`/`0x21`. For other states, the same `GameScene` branch can reach `setPlayerStateWithIdx`, but this function falls back to non-indexed `play()`.

### State groups that clear three MEPlayer fields

Before selecting the action object, native resets `MEPlayer + 0x2f4`, `+0x2f8`, and `+0x2fc` to `-1` for these numeric states/ranges:

- `0x02..0x0a`
- `0x0c..0x10`
- `0x14`
- `0x18..0x1b`
- `0x35..0x36`
- `0x38`

The semantic names of those three fields are not asserted here; the reset contract is directly observed.

### State `0x1c` flip special-case

When the requested state is `0x1c`, the function toggles the stored player flip selector at `MEPlayer + 0x12c` and reconciles it with `MeActionsSystem::isFlipAction()` before continuing. Other states preserve/apply the existing selector in the simpler path.

## Enemy online-state bridge

`EnemyObject::onlineEnemyState` stores the active state at `EnemyObject + 0x274` and uses `0x2f` as a sentinel/suppressed state value in the adjacent state bookkeeping (`+0x278`). It ignores an incoming state if:

- the requested state itself is `0x2f`; or
- it is already the active state.

The function stops/removes the previous `EnemyActionsSystem` at `EnemyObject + 0x10c`, selects the action object associated with the new state from the enemy action array, attaches it, plays it through one of the `EnemyActionsSystem::play` variants, applies facing, refreshes action position/bounds, and triggers state-dependent sound/physics handling.

### Death state proven by health path

`EnemyObject::setOlEnemyHealth` calls:

```text
onlineEnemyState(0x15, true)
```

when online-synchronized health reaches `<= 0`.

At the end of `onlineEnemyState`, state `0x15` is also the only tested value that passes `true` to `EnemyObject::PhysicsSmash(bool)` in this path. Therefore `0x15` is a proven death/smash state for the online-health transition, independent of any guessed enum label.

Transitions to state `0x15` or `0x16` also clear byte `AppParameters + 0x1f8`, showing that both belong to a terminal/special transition family, but only `0x15` is directly selected by the recovered online-health death path.

## Practical reconstruction contract

For the clean-room online state bridge, the currently proven behavior is sufficient to preserve these details:

1. do not restart identical non-movement remote-player states;
2. re-apply position for identical self-movement states;
3. treat `sGotoFrameIdx` as indexed playback only for state `0x20` or `0x21` and nonzero index;
4. preserve the `0x2f` enemy sentinel behavior;
5. map online enemy HP `<= 0` to state `0x15` and the smash/death path.

Full symbolic names for every `MEPLAYERSTATE` / `ENEMYSTATE` numeric value remain a separate reconstruction task; this note intentionally records only behavior proven by the native control flow.
