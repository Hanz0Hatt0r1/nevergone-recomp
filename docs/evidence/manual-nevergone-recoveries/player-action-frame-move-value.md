# Manual reconstruction: player action-frame movement value

> Address convention: RevEngInstr executable addresses use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Functions

| Function | RevEngInstr entry | ELF VMA | P-code ops | Calls | Branches |
| --- | ---: | ---: | ---: | ---: | ---: |
| `MeActionsSystem::updateActionFrameMoveValue` | `0x003393da` | `0x003293da` | 45 | 8 | 3 |
| `MeActionsSystem::curMoveValue` | `0x003393b4` | `0x003293b4` | 11 | 3 | 0 |

Both corpus rows are complete. Ghidra decompilation and Thumb code agree on
the fields and arithmetic.

## Accumulator

When current frame `+0x1a4` differs from the last measured frame `+0x1d8`,
`updateActionFrameMoveValue()` reads the primary sprite `+0x118` bounding box.
It stores the point `(bounds.x + bounds.width / 2, bounds.y)` at `+0x1dc`.
Frame zero establishes this baseline. Later frames add the point difference to
the accumulator at `+0x1e4`, update the baseline, and store the new frame index.

`curMoveValue()` copies `+0x1e4` to its return value and then sets `+0x1e4` to
`(0, 0)`. It is a one-shot read; repeated reads before the next frame update
return zero.

## Runtime connection

The direct renderer already computes the exact full-frame `sprBoundingBox()`
used by the player hit-point reconstruction. On each newly rendered action
frame, `me_actions_live_playback` now updates the same center-X/origin-Y delta
and keeps the native one-shot accumulator. `MEPlayer::playerMoveValue()` consumes
its X component for the live action states `0x02`, `0x10`, and `0x14`; the native
function discards the returned Y component on those paths.

Playback restarts reset the accumulator at frame zero. Remaining movement
branches for knockback, jump-direction flag `+0x3c5`, and MEPlayer flag `+0x150`
remain separate work.

The function-role labels are recorded in
`player-action-frame-move-value-labels.jsonl`. The corpus and generated dataset
remain outside Git.
