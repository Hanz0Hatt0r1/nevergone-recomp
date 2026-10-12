# `klhuo-1.png` SceneActions pair — 2026-10-12

## Clean-room scope

This note records bounded behavior recovered from the shipped Never Gone 1.0.9 ARMv7 `GameSceneObject::initWithData()` path. No original game assets, binary, disassembly listing, or action payload is committed.

## Exact-name branch

For serialized type-0 resource name `klhuo-1.png`, the original does not create the ordinary static sprite. It selects one numeric SceneActions variant and constructs two `SceneActionsSystem` objects using:

- `se01_born%02d.actData`;
- `se01_i%02d.actData`.

The born object is created first and receives byte `SceneActionsSystem +0x24e = 0`. The idle object is created second and receives `+0x24e = 1`. Both are then appended to the owning `GameSceneObject` action-system array in that order.

## Variant calculation

Immediately before formatting those names, native obtains one non-negative 31-bit PRNG result and executes the equivalent ARM/VFP sequence:

```text
normalized = float32(signed_raw / 2^31)
scaled     = float32(normalized * 3.0f)
variant    = trunc_to_i32(scaled) + 1
```

The exact implementation uses `VCVT.F32.S32 ... #31`, a float32 multiply by `3.0f`, then `VCVT.S32.F32`.

This is deliberately **not** modeled as `% 3 + 1`. Float32 rounding at the top of the 31-bit range matters: values from `2147483584` through `2147483647` convert to `1.0f`, producing the rare numeric variant `4`. Project code therefore preserves the machine formula and formats `born04/i04` when that draw occurs; later asset staging must fail closed if the user's imported data does not contain that particular pair.

## Current project boundary

`game_scene_type0_action_pair.h` now provides:

- exact float32 variant selection from one recovered 31-bit draw;
- deterministic formatting of the born/idle action filenames;
- recovered `+0x24e` mode bytes;
- recovered construction order.

This increment does not yet claim full `SceneActionsSystem` parsing/playback semantics or invent the original global PRNG seed/order. Those remain separate runtime integration boundaries.
