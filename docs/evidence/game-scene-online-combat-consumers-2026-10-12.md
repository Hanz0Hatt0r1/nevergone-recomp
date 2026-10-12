# GameScene online-combat consumers — 2026-10-12

## Scope and provenance

Static reconstruction from the user-owned NeverGone 1.0.9 ARMv7 `libcocos2dcpp.so`.

- native SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`
- input path: [`pve-combat-receive-dispatch-2026-10-12.md`](pve-combat-receive-dispatch-2026-10-12.md)
- `GameScene::olUpdataPlayer2Health` — `0x003409fc`
- `GameScene::olUpdataEnemyHealth` — `0x00340a1e`
- `GameScene::olUpdataEnemyState` — `0x00340aa8`
- `GameScene::olUpdataPlayer2State` — `0x00343ad4`

The original native binary is not committed.

## Remote-player health

`GameScene::olUpdataPlayer2Health(OLPlayerHealthData)`:

1. resolves the online/team player through `MEPlayerManager::sharedMEPlayerManager(this, 0)`;
2. reads the `float` health at `OLPlayerHealthData + 0x4`;
3. converts it to signed integer with `vcvt.s32.f32`;
4. calls `MEPlayer::setHealth_T(int)` on the player object at manager slot `+0x108`.

So the wire value remains float through protobuf/OL conversion, but the gameplay health consumer truncates it to an integer.

## Enemy health: message 11 is damage, message 12 is absolute HP

`GameScene::olUpdataEnemyHealth(OLEnemyHealthData, int mode)` first walks `BattleManager` enemies and matches the packet `oid` (`OL + 0x0a`) against `EnemyObject + 0x2ac`. Matching live enemies receive:

```text
EnemyObject::setOlEnemyHealth(int(oHealth), mode)
```

The same packet is then applied to the persistent/current-level action record returned by `GameLevels::GetGLActionsDataWithIdx(oid)`.

### mode 1 (PVE message ID 11)

For the level action record:

```text
new_saved_health = int(float(saved_health) - packet.oHealth)
new_saved_health = max(new_saved_health, 0)
```

Inside `EnemyObject::setOlEnemyHealth(int damage, 1)` at `0x002b0870`:

- current live HP is the float at `EnemyObject + 0x164`;
- incoming value is treated as **damage**, not absolute HP;
- live HP becomes `current - damage`;
- the value sent to `EnemyPhysical::updatePhysicalState(float)` is clamped to at most the pre-hit current HP, so overkill does not report more physical damage than remaining health;
- if resulting HP is `<= 0`, HP is set to zero and `EnemyObject::onlineEnemyState(0x15, true)` is invoked.

Therefore original PVE message ID **11** is the harm/damage update path.

### mode 2 (PVE message ID 12)

For the level action record:

```text
new_saved_health = int(packet.oHealth)
new_saved_health = max(new_saved_health, 0)
```

Inside `EnemyObject::setOlEnemyHealth(int absolute_health, 2)`:

- live HP is replaced by the supplied absolute value;
- if the new value is below the previous value, `EnemyPhysical::updatePhysicalState` receives the positive difference `old - new`;
- if HP stayed equal or increased, physical damage delta is zero;
- the same `<= 0` death transition to online state `0x15` is applied.

Therefore original PVE message ID **12** is the absolute enemy-health synchronization path.

## Remote-player action/state

`GameScene::olUpdataPlayer2State(OLPlayerActionData)` first requires `GameScene::isOnlineState()`.

It resolves the team/remote player and then consumes the reconstructed OL fields as follows:

| OL field | Effect |
| --- | --- |
| `sid` | copied into `ManagementLayer + 0x838` and `MEPlayer + 0x160`; compared with the current local scene ID |
| `x`, `y` | remote-player position |
| `os` | `MEPLAYERSTATE` argument |
| `fd` | horizontal facing; only value `1` maps to `true` |
| `fbfh` | copied to `MEPlayer + 0x3cc` |
| `sGotoFrame` | used as a zero/nonzero selector for indexed-state playback |
| `sGotoFrameIdx` | index passed to `MEPlayer::setPlayerStateWithIdx` when `sGotoFrame != 0` |

When `sid` equals the current scene ID, the exact call sequence is:

```text
MEPlayer::movePlayer_T(CCPoint(x, y), os)
MEPlayer::filpPlayer(fd == 1, os)
MEPlayer+0x3cc = fbfh
if (sGotoFrame != 0)
    MEPlayer::setPlayerStateWithIdx(os, sGotoFrameIdx)
else
    MEPlayer::setTeamActionState(os, true)
```

When the remote `sid` differs from the current scene, the function only calls:

```text
MEPlayer::movePlayer(CCPoint(x, y))
```

This means `sGotoFrame` itself is not passed as the frame number in this consumer; it acts as the branch condition, while `sGotoFrameIdx` is the value passed to the indexed-state method.

## Enemy action/state

`GameScene::olUpdataEnemyState(OLEnemyActionData)` applies a packet in two places: the currently instantiated `EnemyObject`, when present, and the `GameLevelsActionsLayoutData` record used to retain scene state.

### Existing live enemy

The function walks `BattleManager::enemyCount()` and matches packet `oid` against `EnemyObject + 0x2ac`. For every match it calls:

```text
EnemyObject::setEnemyPos(CCPoint(x, y), os)
EnemyObject::flipEnemy(fd == 1)
EnemyObject::onlineEnemyState(os, true)
EnemyObject::onlineTargetPos(CCPoint(tx, ty))
```

Thus:

- `oid` is the identity key used to find the live enemy;
- `os` is the online enemy state;
- `fd` is facing;
- `(x, y)` is current position;
- `(tx, ty)` is online target position.

### Existing level action record

If `GameLevels::GetGLActionsDataWithIdx(oid)` returns a record, the packet updates these recovered fields:

```text
record + 0x24 = CCPoint(x, y)
record + 0x3c = fd
record + 0x40 = CCPoint(tx, ty)
record + 0x48 = os
```

Health is not changed by this action packet; health synchronization is handled by IDs 11/12.

### Missing level action record

If no record exists and packet `sid` differs from the current online scene marker at `ManagementLayer + 0x838`, native creates a new `GameLevelsActionsLayoutData` and adds it with `GameLevels::addEnemyToCurScene(...)`.

The directly proven packet-derived fields on that new record are:

```text
record + 0x20 = enemyId
record + 0x24 = CCPoint(tx, ty)
record + 0x2c = 0
record + 0x34 = oid
record + 0x38 = int(oHealth)
record + 0x48 = os
```

The constructor path also creates/retains two `CCString` values before these writes; their higher-level meaning is not asserted here because this slice alone does not prove a stable semantic name for them.

## Resulting reconstructed semantics

The online combat path can now be represented without relying on guessed message names:

- ID 9: enemy transform/state/target synchronization, keyed by `oid`;
- ID 10: remote-player scene/state/position/facing/action-frame synchronization;
- ID 11: enemy **damage** event;
- ID 12: enemy **absolute HP** synchronization;
- ID 13: remote-player absolute health synchronization.

This also explains why IDs 11 and 12 share the same protobuf message type but must remain distinct through `netBaseData`: the distinction is consumed only at `GameScene::olUpdataEnemyHealth(..., mode)`.

## Next reconstruction boundary

The next useful slice is the state enum / animation contract behind `MEPlayer::setTeamActionState`, `MEPlayer::setPlayerStateWithIdx`, `EnemyObject::onlineEnemyState`, and the death state `0x15`. Recovering those mappings will connect PVE state numbers to concrete action-system animations and hit/death transitions already reconstructed elsewhere in the project.
