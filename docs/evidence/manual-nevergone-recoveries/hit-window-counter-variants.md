# Manual reconstruction: enemy and projectile hit-window counters

> RevEngInstr addresses below are `0x10000` above the shipped ELF VMA. Gold
> label entries retain the RevEngInstr address.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

The ranked retrieval results placed these methods beside the already recovered
`MEPlayer` and `EnemyObject` hit-window methods. Their original Thumb
instructions confirm the same null handling and copy/compare behavior, with
class-specific object offsets.

## Object layouts

| Class | Action-system pointer | Saved hit counter | Action hit counter | Saved combo counter | Action combo counter |
| --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject` | `+0x10c` | `+0x16c` | `+0x2a4` | `+0x168` | `+0x2a0` |
| `LansquenetObject` | `+0x178` | `+0x1ec` | `+0x2a4` | `+0x1e8` | `+0x2a0` |
| `ShootObject` | `+0x124` | `+0x120` | `+0x2b8` | — | — |

For a present action-system pointer, `isHitFlag()` and `isComboHit()` compare
the saved counter with the corresponding action counter and return whether
they differ. `addHitFlagCount()` and `addComboHitCount()` copy the current
counter into the saved field. `tempHitFlagCount()` and
`tempComboHitCount()` return the live action counter. A null pointer makes the
gate return true, the add method leave the saved field untouched, and the temp
method return zero. `ShootObject` implements only the hit-counter trio here.

## Confirmed methods

| Function | RevEngInstr entry | Recovered role |
| --- | ---: | --- |
| `EnemyObject::isComboHit` | `0x002bfa8c` | `combat_combo_hit_window_gate` |
| `EnemyObject::addComboHitCount` | `0x002bfaa6` | `combat_combo_hit_window_advance` |
| `EnemyObject::tempComboHitCount` | `0x002bfab6` | `combat_combo_hit_window_counter_read` |
| `LansquenetObject::isHitFlag` | `0x002eb310` | `combat_hit_window_gate` |
| `LansquenetObject::addHitFlagCount` | `0x002eb32a` | `combat_hit_window_advance` |
| `LansquenetObject::tempHitFlagCount` | `0x002eb33a` | `combat_hit_window_counter_read` |
| `LansquenetObject::isComboHit` | `0x002eb352` | `combat_combo_hit_window_gate` |
| `LansquenetObject::addComboHitCount` | `0x002eb36c` | `combat_combo_hit_window_advance` |
| `LansquenetObject::tempComboHitCount` | `0x002eb37c` | `combat_combo_hit_window_counter_read` |
| `ShootObject::isHitFlag` | `0x003694fc` | `combat_hit_window_gate` |
| `ShootObject::addHitFlagCount` | `0x00369516` | `combat_hit_window_advance` |
| `ShootObject::tempHitFlagCount` | `0x00369526` | `combat_hit_window_counter_read` |

The methods are short Thumb bodies. For example, `LansquenetObject::addHitFlagCount`
loads `this+0x178`, returns on null, loads action-system `+0x2a4`, and stores it
to `this+0x1ec`; its combo variant uses `+0x2a0` and `this+0x1e8`. The
`ShootObject` variant instead reads the pointer at `+0x124`, live hit counter at
action-system `+0x2b8`, and saved counter at `+0x120`.

## Reconstruction

`hit_window_counter_contract` now implements the shared null, compare, copy,
and read semantics. The existing `player_hit_window_contract` and
`enemy_hit_gate_contract` adapt their recovered class fields to it; the enemy
contract now also exposes the recovered combo gate, advance, and counter read.
The layout table stays explicit so callers cannot silently apply one actor's
offsets to another class. This contract does not infer projectile collision or
damage dispatch.

The 12 gold function-role labels are recorded in
`hit-window-counter-variants-labels.jsonl` and merged into the local aggregate
by `tools/ml/merge_nevergone_manual_labels.py`.
