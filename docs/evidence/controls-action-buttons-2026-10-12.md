# Controls action buttons — 2026-10-12

## Clean-room boundary

This note records bounded static behavior recovered from the shipped ARMv7 `libcocos2dcpp.so`. No proprietary binary, screenshot, decoded asset, heap dump, or disassembly listing is committed.

## Construction

`Controls::init()` (ELF `0x00297e2c`) constructs three `ControlsButton` instances after the joystick:

| Control | Normal sprite | Selected sprite | Controls field | Tag | Default center | `CONTROLSSTATE` |
| --- | --- | --- | --- | ---: | --- | ---: |
| A | `ab.png` | `ab_s.png` | `+0x12c` | 1 | `(visibleWidth - 238, 90)` | 7 |
| B | `db.png` | `db_s.png` | `+0x130` | 2 | `(visibleWidth - 96, 90)` | 8 |
| C / jump | `jb.png` | `jb_s.png` | `+0x134` | 3 | `(visibleWidth - 136, 224)` | 9 |

The nearby `a_to_j_tip.png` is created separately and is not an action-button hit target.

The default centers are direct float literals in `Controls::init()`: `238`, `96`, `136`, `224`, and `90`. The same function also supports user-stored control positions; this contract intentionally represents only the shipped defaults until the position-settings bridge is reconstructed.

## Hit testing

`ControlsButton::touchBegin`, `touchMoved`, and `touchEnd` share the same important geometry rules:

1. retrieve the visible size;
2. reject the action-button path when touch `x <= visibleWidth * 0.5`;
3. retrieve the normal sprite at `ControlsButton + 0x104`;
4. call that sprite's Cocos `boundingBox()`;
5. call `CCRect::containsPoint(...)` for the converted local touch coordinate.

Therefore the original does **not** use a guessed circular radius for A/B/C. The reconstructed hit box must be derived from the decoded original normal sprite dimensions. `game_scene_action_layout` accepts those dimensions at runtime and mirrors the default centered sprite bounding-box contract.

## Callback and state evidence

The shipped native exports include:

- `Controls::menuABCallback()` — ELF `0x002966e0`;
- `Controls::menuDBCallback()` — ELF `0x002966e8`;
- `Controls::menuJCallback()` — ELF `0x002966f0`;
- `Controls::OnHandleButton(CONTROLSSTATE, bool)` — ELF `0x00297a7c`;
- `ControlsButton::touchBegin(...)` — ELF `0x002986b8`;
- `ControlsButton::touchMoved(...)` — ELF `0x002987d2`;
- `ControlsButton::touchEnd(...)` — ELF `0x002988e2`.

`Controls::OnHandleButton` provides the direct state-ID mapping. On the pressed path:

- state `7` selects the button retained at `Controls + 0x12c` (A) and sets the matching retained pressed flag;
- state `8` does the same for `Controls + 0x130` (B);
- state `9` does the same for `Controls + 0x134` (C / jump).

The release path tests the same state values, calls `ControlsButton::unselected()` on the same three retained button pointers, and clears the corresponding retained pressed flags. This proves `7/8/9` as the original action-button `CONTROLSSTATE` IDs without inferring their combat semantics from names alone.

The three menu callbacks clear the same retained Controls field at `+0x118`; the detailed downstream player-action meaning of that field remains a separate reconstruction boundary.

## Project boundary

`game_scene_action_layout` owns:

- resource/tag/default-center mapping;
- sprite-bounds-derived rectangles;
- original right-half rejection rule;
- deterministic A/B/C hit classification.

`game_scene_action_input` additionally exposes the recovered reversible mapping:

```text
A <-> CONTROLSSTATE 7
B <-> CONTROLSSTATE 8
C <-> CONTROLSSTATE 9
```

Neither layer fabricates sprite dimensions or player combat outcomes. The next integration should obtain decoded `ab/db/jb` dimensions from imported user-owned resources, classify the Android touch through `game_scene_action_layout`, then forward the control and recovered state ID into the runtime action dispatch.
