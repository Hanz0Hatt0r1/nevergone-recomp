# PLAYERFACEDIR field and control-state evidence

The shipped 1.0.9 ARMv7 `libcocos2dcpp.so` used for this reconstruction has
SHA-256:

```text
94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e
```

A bounded Thumb disassembly closes the previously separate integer
`PLAYERFACEDIR` question without assigning enum values by convention.

## Shared MEPlayer field

At ELF VA `0x0031c75a`, `MEPlayer::setPlayerDir(PLAYERFACEDIR)` performs:

```text
cmp   r1, #1
str.w r1, [r0, #0x12c]
...
```

so the integer enum argument is stored directly at `MEPlayer +0x12c`.

At ELF VA `0x0031c748`, `MEPlayer::isFace()` loads exactly the same field:

```text
ldr.w r0, [r0, #0x12c]
...
```

and returns true only when that value is `1`.

At ELF VA `0x0031a4ec`, `MEPlayer::filpPlayer(bool, MEPLAYERSTATE)` writes that
same field from its boolean argument:

```text
cbz   r1, false_path
movs  r3, #1
str.w r3, [r4, #0x12c]
...
false_path:
str.w r1, [r4, #0x12c]
```

Therefore the bounded facing path uses the same storage for the boolean facing
state and `PLAYERFACEDIR`; for this path the exact integer values are `1` for
true and `0` for false.

The existing Ghidra project is rebased by `+0x10000`, so the corresponding
analyzed-image addresses are `0x0032c748`, `0x0032c75a`, and `0x0032a4ec`.

## Primary Controls mapping

`MEPlayer::setControlsState(CONTROLSSTATE)` begins at ELF VA `0x0031ddee`.
The already recovered branches are directly visible in the same binary:

```text
0x0031de36: cmp r5, #0x12
...
0x0031de42: movs r1, #1
0x0031de44: bl MEPlayer::filpPlayer

0x0031de50: cmp r5, #0x13
...
0x0031de5e: movs r1, #0
0x0031de60: bl MEPlayer::filpPlayer
```

The later facing-maintenance path repeats the same contract around ELF
`0x0031df3e` / `0x0031df4c`: state `0x12` supplies true and state `0x13`
supplies false.

Thus for the reconstructed horizontal primary states:

```text
CONTROLSSTATE 0x12 -> flip_x=true  -> PLAYERFACEDIR 1
CONTROLSSTATE 0x13 -> flip_x=false -> PLAYERFACEDIR 0
```

The Ghidra-image equivalents of the setControlsState anchors are
`0x0032de36`, `0x0032de50`, `0x0032df3e`, and `0x0032df4c`.

## Production provider

`tutorial_player_face_dir_provider` consumes the retained
`player_control_runtime_state::Snapshot` and emits a generation-stamped
`ReviewedPlayerFaceDir` only when:

- one of the two recovered primary controls states is present; and
- the stored `flip_x` agrees with `player_control_contract::facing_flip_x()`
  for that state.

An inconsistent snapshot fails closed. No default direction is supplied before
horizontal facing has been established.

The older `player_face_dir_call_evidence.py` / Ghidra P-code workflow remains
useful as an independent cross-check, but it is no longer required to guess or
assign the enum values: the shared `+0x12c` storage proves the bounded runtime
mapping directly.
