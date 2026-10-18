# Manual reconstruction: enemy target position

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function identity

The recovered overload is
`EnemyObject::actionTargetPos(ENEMYSTATE, MEPlayer*, cocos2d::CCPoint, bool, bool)`.
Its RevEngInstr/Ghidra image address is `0x002c188c`; the ELF function starts
at `0x002b188c` (the dynamic Thumb symbol is `0x002b188d`). The function body
is 320 bytes. The source is the read-only Ghidra function export for the same
binary; no original binary or full pseudocode dump is stored in the repository.

## Recovered control flow

The function first queries `EnemyObject::isEnemyMoveState()`. This note was
written from the decompiler export, which left the randomized-X bounds and the
unsigned distance predicate unresolved. Those values are now recovered from
the complete Thumb body in
[`enemy-object-action-target-position.md`](enemy-object-action-target-position.md);
that instruction-level note supersedes the unresolved-value section below.

- When it returns false, the function calls `enemyOrgHLPos()`, computes a
  `ccpDistance` involving the supplied point values, and returns when the
  integer-converted distance is below `0x51` (81). Otherwise execution reaches
  the common `flipEnemy()` call.
- When it returns true, it constructs a point from `EnemyObject +0x17c`, calls
  `MEPlayer::playerHLPoint()`, and computes the distance between those values.
  The exact unsigned branch condition is recorded in the instruction-level
  note linked above.
- The boolean gate starts with the original fifth argument. If the sixth
  argument is false, it replaces the fifth argument with
  `fifth_argument || distance_condition`; if the sixth argument is true, the
  fifth argument is left unchanged. A true result selects a randomized target-X
  write at `EnemyObject +0x120` and copies the supplied point's Y into `+0x124`.
  The randomized branch also calls `flipEnemy()` and returns. A false result
  copies the supplied `CCPoint` to `+0x120`, then returns without the final flip
  for states `0x2b` and `0x1b`; other states reach `flipEnemy()`.

These branches establish that `+0x120/+0x124` are the target-position fields
updated by this overload, and that states `0x2b` and `0x1b` have special
post-copy return behavior. They complement the existing evidence that those
states bypass the ordinary target update in native `actionTargetPos`.

## Earlier unresolved values

The two boolean parameters remain unnamed. The Thumb constants and randomized-X
bounds have since been resolved; see the instruction-level note. Caller-owned
inputs are still required before this function can drive live enemy movement.

Related evidence: [`online-enemy-action-index-2026-10-17.md`](../online-enemy-action-index-2026-10-17.md)
and [`enemy03-ai-action-routes-2026-10-17.md`](../enemy03-ai-action-routes-2026-10-17.md).
