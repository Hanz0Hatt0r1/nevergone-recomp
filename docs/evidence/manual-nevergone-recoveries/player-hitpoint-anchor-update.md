# Manual reconstruction: player hit-point anchor update

> RevEngInstr executable addresses use an image base `0x10000` above the
> shipped ELF VMA. Gold-label `source_entry` retains the corpus address.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MeActionsSystem::actionBoundingBox` | `0x00339350` | 11 | 3 | 2 | 1 | 0 |
| `MEPlayer::playerBoundingBox` | `0x00328b5e` | 35 | 4 | 5 | 2 | 1 |
| `MEPlayer::updateHLValue` | `0x00328c24` | 78 | 3 | 19 | 2 | 1 |

All three corpus rows are complete. Thumb disassembly confirms the following
coordinate flow.

## Bounding-box source and translation

`MeActionsSystem::actionBoundingBox()` returns the primary sprite's
`sprBoundingBox()` from `MeActionsSystem +0x118`. When the sprite pointer is
null, it returns `(0, 0, 1, 1)`.

`MEPlayer::playerBoundingBox()` preserves the action rectangle's width and
height while choosing its origin by action state:

- State `0x11` and states `0x2f..0x30`: x is `+0x240.x + +0x248.x - width/2`;
  y is `+0x240.y + +0x248.y + actionRect.y`.
- States `0` and `1`: on the first call, it caches the action rectangle at
  `+0x3f0` and marks `+0x400`. It uses the current rectangle's width and the
  cached rectangle's y with the same `+0x240/+0x248` translation.
- Other states: it adds the action rectangle origin to the point at `+0x268`.

## Hit-point anchor update

`MEPlayer::updateHLValue()` computes `(playerRect.x + width/2, playerRect.y)`.
It writes only the x float at `+0x130` for these state sets, preserving the
previous y float:

- When the raw selector at `MEPlayer +0x144` equals `1`: `0x19..0x1a`,
  `0x35..0x36`, `0x04..0x0a`, `0x0f`, `0x20..0x21`, `0x31`, `0x17`,
  `0x11..0x13`.
- For other selector values: `0x19..0x1a`, `0x35..0x36`, `0x06`, `0x0f`,
  `0x20..0x21`, `0x31`, `0x38`, `0x12..0x13`, `0x17`, `0x0b..0x0c`.

All other states replace both coordinates at `+0x130`. The
`MEPlayer::playerHLPoint()` accessor returns that point, which feeds the
recovered player/enemy intersection gate. `player_hl_point_contract` models the
three-function chain and returns the updated movement-state cache explicitly.
Sprite transformed bounds and the native field inputs remain supplied at the
contract boundary; this recovery does not infer a texture rectangle from raw
Section-E positions.

Function-role labels are recorded in `player-hitpoint-anchor-update-labels.jsonl`
and added to local manual-restoration dataset v48. It now contains 9,199
labelled functions, 6,907 structural groups, and 9,821 gold label rows, with
splits of 7,389/818/992. The generated corpus and dataset remain outside Git.
