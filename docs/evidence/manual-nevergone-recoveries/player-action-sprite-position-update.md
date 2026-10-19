# Manual reconstruction: player action sprite position update

> RevEngInstr executable addresses use an image base `0x10000` above the
> shipped ELF VMA. Gold-label `source_entry` retains the corpus address;
> subtract `0x10000` to inspect the ELF.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MeActionsSystem::swapBufferPos` | `0x003396e0` | 703 (256 emitted) | 81 | 62 | 102 | 0 |

The RevEngInstr function is truncated at its 256-op model-text limit. The
complete Thumb body was inspected locally at ELF VMA `0x3296e0..0x329d32`.

## Section-E hit-point update

The function copies its `CCPoint` argument to `MeActionsSystem +0x2ec`, then
scans `MeActionsData +0xc8` in source order. For each Section-E record whose
`+0x74` frame index equals `MeActionsSystem +0x1a4`, it gets the sprite at the
same index from `MeActionsSystem +0x198` and calls `setPosition()` with:

```text
x = ActionFrameData +0x80 - MeActionsSystem +0x2ec.x
y = ActionFrameData +0x84 - MeActionsSystem +0x2ec.y
```

The Section-E loop does not perform a separate sprite-array length check before
that same-index lookup. The reconstruction emits matching record candidates
with their frame ordinal and positions in action-system space; it does not claim
that every record produced a native sprite, because the sprite-loading
predicate has not been recovered.

`game_scene_player_render_queue::with_live_player()` now adds the candidates for
the selected live action frame to `Queue::player_action_hitpoints`. The runtime
state carries the two `+0x2ec` offsets explicitly. The current project-owned
playback path uses the recovered zero-offset play baseline; the player movement
cursor is kept separate because equivalence to `MeActionsSystem +0x2ec` has not
been established.

This recovers the Section-E position update only. Player collision still needs
the action sprites' transformed bounds centers consumed by
`MEPlayer::beatHitPoint()`; those are not inferred from these record positions.
The gold player action sprite-position function-role label is recorded in
`player-action-sprite-position-update-labels.jsonl` and added to the local
manual-restoration dataset v47. That adds one gold row, bringing it to 9,196
labelled functions and 6,904 structural groups, with splits of 7,386/818/992.
Generated corpus and dataset files remain outside Git.
