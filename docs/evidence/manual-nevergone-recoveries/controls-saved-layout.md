# Manual reconstruction: saved control layout

This recovery uses the user-owned ARMv7 `libcocos2dcpp.so` analyzed in Ghidra.
It implements the `Controls::init()` settings branch and applies one imported
Android SharedPreferences XML file at runtime. No original preference file or
proprietary art is committed.

## Recovered settings

`Controls::init()` at `0x002a7e2c` reads `controls_settings` and, when enabled,
loads these float pairs through `CCUserDefault`:

| Keys | Recovered use |
| --- | --- |
| `joyStickX`, `joyStickY` | joystick center |
| `BX`, `BY` | B button center |
| `uniqueSkillX`, `uniqueSkillY` | `a_to_j_tip.png` center and A/C layout anchor |

The anchor moves A and C together: A is `(uniqueSkillX - 51,
uniqueSkillY - 64)` and C is `(uniqueSkillX + 51, uniqueSkillY + 64)`. These
offsets are direct ARM float literals and the two points are assigned to the
same stack positions later passed to `ControlsButton::setSprPosition()`.

With no saved layout, the original centers are joystick `(204,173)`, A
`(visibleWidth - 238,90)`, B `(visibleWidth - 96,90)`, C
`(visibleWidth - 136,224)`, and the hidden `a_to_j_tip` sprite
`(visibleWidth - 187,154)`.

## Runtime integration

The Android developer-tools panel accepts the exported SharedPreferences XML.
The loader reads the recovered layout settings, rejects incomplete or
non-finite custom coordinates, and returns to the shipped defaults on invalid
input. The resulting positions are shared by joystick hit testing, joystick
rendering, action-button hit testing and rendering, and tutorial control cues.
Action-button bounds still use the imported normal sprite dimensions and retain
the original left-half rejection rule.

The developer-tools panel also provides a draggable editor for the joystick,
A/B/C controls, and two quick-use slots. It saves all ten recovered coordinate
values in Android SharedPreferences XML form. A and C remain paired around
the recovered `uniqueSkillX/Y` anchor. This is a partial reconstruction of
`SetControlPosition`: collision rejection and quick-use item actions remain
outstanding; see [`control-layout-editor.md`](control-layout-editor.md).

## Corpus label

[`controls-saved-layout-labels.jsonl`](controls-saved-layout-labels.jsonl)
adds `Controls::init()` as a gold `ui_renderer` example. The label is supported
by direct asset construction, layout coordinates, saved-setting key references,
and the new runtime reconstruction.
