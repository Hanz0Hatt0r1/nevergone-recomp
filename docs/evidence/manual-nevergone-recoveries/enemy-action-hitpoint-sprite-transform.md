# Manual reconstruction: enemy Section-E hitpoint sprite transforms

> RevEngInstr executable addresses use an image base `0x10000` above the
> shipped ELF VMA. Subtract `0x10000` to inspect the ARMv7 ELF.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Relevant RevEngInstr functions

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `EnemyActionsSystem::updateActionFrameMoveValue` | `0x002bc28e` | 98 | 7 | 6 | 10 | 3 |
| `EnemyActionsSystem::swapBufferPos` | `0x002bbf3a` | 325 (256 emitted) | 26 | 23 | 37 | 0 |

`updateActionFrameMoveValue` is complete in the RevEngInstr corpus. The
`swapBufferPos` body is truncated there, so its complete local Thumb disassembly
at ELF VMA `0x2abf3a..0x2ac154` supplies the remaining evidence. The Section-E
transform is at `0x2ace30..0x2acfa4` in the complete Thumb body of
`updateActionFrameMoveValue`.

## Native Section-E transform

For each fixed-tail `ActionFrameData`, native selects rows whose `+0x74` frame
index equals `EnemyActionsSystem+0x294`, then uses the same array index in the
parallel sprite array at `EnemyActionsSystem+0x150`. It sets the sprite
position to:

```text
x = ActionFrameData+0x14 + EnemyActionsSystem+0x194
y = ActionFrameData+0x18 + EnemyActionsSystem+0x198 - EnemyActionsSystem+0x280
```

The fixed-tail parser maps those position fields to serialized `f0/f1`, anchor
to `f2/f3`, pivot to `f6/f7`, and scale-X to `f9`. Section E keeps scale-Y at
1 and rotation at 0. Native writes the position into the system's `+0x240`
point and the row's `ActionFrameData+0x80/+0x84`, sets anchor and scale-X, then
`swapBufferPos()` updates the sprite to:

```text
x = ActionFrameData+0x80 - EnemyActionsSystem+0x284.x
y = ActionFrameData+0x84
```

When `EnemyActionsSystem+0x16a` is set, native repeats the initial position
calculation, mirrors it around pivot `(f6,f7)` through point subtraction,
Y-delta negation, and point addition, mirrors anchor-X with
`0.5 - (f2 - 0.5)`, and negates scale-X. The action-system offset from
`swapBufferPos()` is subtracted after this transform.

`assets/hitPoint.png` decodes to the validated native 8x8 image used by the
player Section-E path. The reconstructed transform is passed through the shared
Cocos sprite-bounds helper, which transforms all four untrimmed image corners
before taking the AABB center. This matches the center returned by native
`EnemyActionsSystem::GetCurFrameHitPoint()` through `CCSprite::sprBoundingBox()`.

## Reconstruction status

`enemy_actions_hitpoint_sprite_position::for_frame()` now derives the enemy
hitpoint sprite position, anchor, scale, supported system flip, and bounds
center from Section-E records. The live queue uses the same system inputs as
the rendered enemy body. On the current action-start path, the
`EnemyActionsSystem+0x284` position input remains `(0,0)`; the queue keeps
nonzero movement and the later `EnemyObject::beatHitPoint()` offsets as
separate inputs instead of guessing them.

The reconstructed WBG document does not yet preserve the original native
sprite-loading predicate, so matching Section-E rows remain candidates. Enemy
damage and player/enemy collision are not connected to these centers yet.

The complete `EnemyActionsSystem::updateActionFrameMoveValue` role is added as
a gold label in `enemy-action-hitpoint-sprite-transform-labels.jsonl`. The
manual-restoration v55 dataset now has 9,211 labelled functions, 6,918
structural groups, and 9,837 gold rows (7,399 train, 819 test, 993
validation). The generated corpus and dataset remain outside Git.
