# Manual reconstruction: player Section-E hitpoint sprite transforms

> RevEngInstr executable addresses use an image base `0x10000` above the
> shipped ELF VMA. Gold-label `source_entry` retains the corpus address;
> subtract `0x10000` to inspect the ELF.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Relevant RevEngInstr functions

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MeActionsSystem::updateData` | `0x00339d34` | 8,054 (256 emitted) | 293 | 115 | 394 | 7 |
| `MeActionsSystem::filpAction` | `0x00339108` | 69 | 5 | 8 | 13 | 1 |
| `MeActionsSystem::setCutActionValue` | `0x0033b914` | 248 | 29 | 19 | 13 | 3 |

`updateData` exceeds the corpus model-text limit; only its complete local
Thumb body and Ghidra decompilation were used for the transform. The two small
state-update helpers are complete corpus rows and are added as gold labels.

## Native Section-E transform

`MeActionsSystem::initWithFile()` creates one CCSprite for each Section-E
record with `ActionFrameData +0x6c > 0`, using its `+0x68` image name, and
stores sprites in the parallel `MeActionsSystem +0x198` array. The fixed-tail
loader stores the current-frame index at `+0x74` and maps serialized values as
f0/f1 -> `+0x14/+0x18`, f2/f3 -> `+0x24/+0x28`, f6/f7 -> `+0x1c/+0x20`, and
f9 -> `+0x34`.

In the Section-E loop at ELF VMA `0x32add4..0x32afa6`, `updateData()` selects
the current `+0x74` row, then sets its sprite position to:

```text
x = (AFD+0x14 + system+0x204.x) / system+0x2f8
y = (AFD+0x18 + system+0x208.y) / system+0x2f8 - system+0x648
```

It also sets the anchor from AFD `+0x24/+0x28` and scale-X from AFD `+0x34`.
The freshly created `hitPoint.png` CCSprite keeps its default scale-Y of 1 and
rotation of 0. The original APK's encoded `assets/hitPoint.png` decodes with
the recovered native asset transform to a valid 8x8 PNG (decoded SHA-256
`474bea19ba8318092653464387f2b4479686e6200a281236a7b843c85043ee3a`).

`MeActionsSystem::filpAction(bool)` writes the system flip flag at `+0x1be`
and applies `CCSprite::setFlipX()` to its child sprites. When the flag is set,
`updateData()` mirrors the sprite position about pivot `(f6,f7)` after dividing
by `+0x2f8`, changes anchor-X to `1 - f2`, and negates scale-X. The Y reflection
preserves the original position Y. `swapBufferPos()` subsequently subtracts
the separate `+0x2ec` point; this remains modeled independently.

The supported `MEPlayer::createActions()->play()` path uses scale 1 and starts
the position and cut offsets at zero. `setCutActionValue()` can set a nonzero
`+0x648` cut offset; that special cut adjustment is not yet supplied by the
reconstructed runtime.

## Reconstruction status

`game_scene_player_render_queue::with_live_player()` now builds the Section-E
sprite transform from the serialized fields, applies the known live facing
state and the separate `swapBufferPos()` offset, and stores the center of the
8x8 sprite bounds. It reuses the existing Cocos sprite-bounds geometry helper,
which transforms all four corners before computing the AABB center, matching
`MeActionsSystem::curFrameHitPoint()`'s `sprBoundingBox()` center calculation.

This supplies the player hitpoint centers to the render queue; `BattleManager`
does not yet consume them for enemy intersection, damage, combo advancement, or
hit reactions. Enemy Section-E transform reconstruction and nonzero player cut
offset handling also remain open.

Gold labels for the complete `filpAction` and `setCutActionValue` functions are
recorded in `player-action-hitpoint-sprite-transform-labels.jsonl`. Adding
those four rows to manual-restoration v53 and rebuilding as v54 produces
9,211 labelled functions, 6,918 structural groups, and 9,836 gold rows (7,399
train, 819 test, 993 validation). The generated corpus and dataset remain
outside Git.
