# Manual reconstruction: enemy hit gates and frame power

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

The local RevEngInstr corpus contains these compact combat functions:

| Function | Entry | P-code ops | Calls | Branches | Loads |
| --- | --- | ---: | ---: | ---: | ---: |
| `EnemyObject::allowHitPlayer()` | `0x002bf096` | 4 | 0 | 0 | 1 |
| `EnemyObject::isHitFlag()` | `0x002beed2` | 16 | 0 | 1 | 3 |
| `EnemyObject::addHitFlagCount()` | `0x002beeec` | 12 | 0 | 1 | 2 |
| `EnemyObject::tempHitFlagCount()` | `0x002beefc` | 12 | 0 | 1 | 2 |
| `EnemyActionsSystem::curFramePower()` | `0x002bdb3c` | 31 | 2 | 2 | 7 |
| `EnemyObject::curFramePower()` | `0x002bfac2` | 9 | 1 | 1 | 1 |

Corpus counts were used only to rank bounded candidates; the field contracts
below come from the original Thumb instructions and corroborating consumers.

## Hit-permission and once-per-window fields

`EnemyObject::allowHitPlayer()` is exactly one byte load from
`EnemyObject +0x244`. Its clean-room helper preserves the native boolean test:
zero is false; any nonzero byte is true.

`EnemyObject::isHitFlag()` at `0x002beed2` has this bounded behavior:

```text
actions = EnemyObject +0x10c
if actions == null: return true
return EnemyObject +0x16c != actions +0x2a4
```

The ARM instructions load those two 32-bit values and test their subtraction
for nonzero. `addHitFlagCount()` copies `actions +0x2a4` into
`EnemyObject +0x16c` when the action-system pointer is nonnull. The companion
`tempHitFlagCount()` returns that same action-system counter, or zero when the
pointer is null. The project-owned `enemy_hit_gate_contract` mirrors those
three state transitions without assigning an unproven semantic name to the
counter.

## Per-frame power

`EnemyActionsSystem::curFramePower()` uses:

1. action data pointer `EnemyActionsSystem +0x108`;
2. unsigned current frame index `EnemyActionsSystem +0x294`;
3. `CCArray` pointer `EnemyActionsData +0x9c`;
4. a frame-index bounds check against that array's count;
5. `ActionComboValue +0x1c` from the selected element, or integer `1` if the
   index is outside the array or `objectAtIndex()` returns null.

`EnemyObject::curFramePower()` delegates through `EnemyObject +0x10c`, and
returns `1` if there is no action system. This is distinct from
`DamageFactor()`: it is the native frame-power argument supplied separately to
the player hit routine.

Section F already provides the selected value. Its one-per-primary-record
`array_9c_values[i].field_1c` is `tuple[i].i32_values[2] + 1`, written into the
original `EnemyActionsData +0x9c` array. `resolve_frame_power()` uses that
evidence-backed element and mirrors the native fallback value `1`.

## Bounded player-hit handoff

The large `BattleManager::enemyHitPlayer()` function at `0x002b48dc` has 3,881
RevEngInstr operations and its exported row is truncated at 256. Ghidra shows
that its bounded enemy-hit branches call both `EnemyObject::inputAtkValue()`
and `EnemyObject::curFramePower()`, then pass those values to
`MEPlayer::isBeaten()`. The helper
`tutorial_attack_live_gate::live_resolve_combat_frame()` now exposes the
generation-guarded pair for one selected action document and frame:

```text
input_attack_value = value_2bc * SectionF.first_float[frame]
frame_power        = SectionF.array_9c_values[frame].field_1c
```

This restores the numeric handoff and hit-window fields only. It does not
recreate `BattleManager::enemyHitPlayer()`'s large state switch, hit-point
collision, player guard/dodge/parry rules, damage/knockback application, or
effects. Those decisions remain under their own evidence boundaries.

The gold function-role labels for the six bounded accessors are recorded in
`enemy-input-attack-value-labels.jsonl`. Rebuilding the local RevEngInstr
dataset as manual-restoration v11 produced 9,131 labelled functions, 6,844
structural groups, and 9,683 gold label rows. The corpus and generated dataset
remain outside Git.
