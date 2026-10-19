# Manual reconstruction: enemy action sprite position update

> Address convention: RevEngInstr executable addresses use an image base
> `0x10000` above the shipped ELF VMA. Gold-label `source_entry` keeps that
> corpus address; subtract `0x10000` to inspect the ELF.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyActionsSystem::swapBufferPos` | `0x002bbf3a` | 325 (256 emitted) | 26 | 23 | 37 | 0 |

The corpus marks this function truncated at the 256-op model-text limit. The
complete Thumb body was inspected locally at ELF VMA `0x2abf3a..0x2ac154`.

## Section-E hit-point sprite update

The method copies its `CCPoint` argument into `EnemyActionsSystem +0x284` and
returns early when the action data, primary-frame array, or relevant object
array is unavailable. It scans `EnemyActionsData +0x84` in source order. For
each record whose `+0x74` frame index equals `EnemyActionsSystem +0x294`, it
checks that the same index exists in the parallel sprite array at
`EnemyActionsSystem +0x150`, then calls that sprite's `setPosition()` with:

```text
x = ActionFrameData +0x80 - EnemyActionsSystem +0x284.x
y = ActionFrameData +0x84
```

The x offset is subtracted; the point's y component is not used by this Section-E
position path. `EnemyActionsSystem::play()` initializes its `+0x284` point to
`(0, 0)`. `EnemyActionsSystem::step(float)` can call this method with `(0, 0)`,
while `EnemyObject::setEnemyMove(CCPoint,bool)` and
`LansquenetObject::setLansquenetMove(CCPoint,bool)` also call it with a point
derived from their movement state.

`enemy_actions_hitpoint_sprite_position::for_frame()` reproduces the recovered
record selection and Section-E transform; see
`enemy-action-hitpoint-sprite-transform.md` for the full bounds-center
calculation. The live enemy render queue carries these centers for the selected
animation frame in `Queue::enemy_action_hitpoints`, keyed by enemy slot and
frame, using the action-start zero offset.

The queue does not apply `EnemyObject::beatHitPoint()`'s later `+0x118` and
`+0x284` translations or a nonzero movement input to
`EnemyActionsSystem::swapBufferPos()`. Because the reconstructed document does
not preserve the native sprite-loading predicate, the entries identify
matching Section-E records rather than asserting that every record created a
sprite in the original `+0x150` array.

`enemy-action-sprite-position-update-labels.jsonl` adds the gold combat
function-role label; the combat subsystem label already existed. Rebuilding
manual-restoration v45 as v46 adds one gold row and keeps 9,195 labelled
functions, 6,903 structural groups, and the existing 7,385/818/992 train/test/
validation split sizes. The corpus and generated dataset remain outside Git.
