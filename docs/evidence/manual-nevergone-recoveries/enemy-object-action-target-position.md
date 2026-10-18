# Manual reconstruction: enemy action target position

> RevEngInstr image addresses are `0x10000` above the shipped ELF VMA.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native function

`EnemyObject::actionTargetPos(ENEMYSTATE, MEPlayer*, CCPoint, bool, bool)`
starts at image address `0x002c188c` / ELF VMA `0x002b188c`. The complete
Thumb body is `0x2b188c..0x2b19ca`; the trailing word at `0x2b19c8` is the
float literal `200.0f` (`00 00 48 43`). The project contract receives the
`isEnemyMoveState()` result, the `MyRandomFloat()` result, and the other
external observations as explicit inputs.

## Moving-state path

When `isEnemyMoveState(state)` is true, native loads the enemy point at
`+0x17c`, reads `MEPlayer::playerHLPoint()`, calls `cocos2d::ccpDistance()`,
converts the result to signed int with truncation toward zero, and compares it
with `4096`.

The two raw boolean arguments are at the caller stack's `+0x30` and `+0x34`.
They control whether native replaces the requested X coordinate with a random
one. In terms of those raw fields, the random branch is selected when:

```text
argument_30 != 0
    or (truncated_distance > 4096 and argument_34 == 0)
```

For a random X, native compares enemy `+0x17c.x` with signed
`EnemyObject+0x2c4 / 2`. If it is above the midpoint, it calls
`MyRandomFloat(200.0f, enemy_x)` and passes `false` to `flipEnemy()`; otherwise
it calls `MyRandomFloat(enemy_x, float(field_2c4))` and passes `true`. The
result is stored at `+0x120`. In both moving-state paths, the supplied point's
Y is stored at `+0x124`.

When X is not randomized, native copies the supplied point into `+0x120/+0x124`
and passes `true` to `flipEnemy()` when `+0x17c.x <= requested_x`, otherwise
`false`. States
`0x2b` and `0x1b` suppress only this direction update; they do not suppress the
target-point write. The contract exposes the `flipEnemy(bool)` argument
without assuming the renderer's flip convention.

## Non-moving-state path

When `isEnemyMoveState(state)` is false, native gets `enemyOrgHLPos()`, measures
its distance to the supplied point, truncates it to int, and returns without a
facing call when the result is at most `80`. Above `80`, it calls
`flipEnemy(true)` when `origin_x <= requested_x` and `flipEnemy(false)`
otherwise. This branch does not write `+0x120/+0x124`.

## Project-owned reconstruction and limits

`enemy_object_action_target_position::resolve()` models both branches without
calling cocos2d. It keeps the two boolean parameters and `+0x2c4` offset as
raw inputs, and accepts the RNG result explicitly. The function is not yet
connected to the live tutorial renderer: the actor binding still lacks the
caller-owned requested point and random-state inputs needed to replay the
original `EnemyObject::eventAction()` lifecycle.

`enemy-object-action-target-position-labels.jsonl` adds one gold function-role
example for the automatic restoration corpus. The original binary and
generated RevEngInstr datasets remain outside Git.
