# GameLevels enter-game boundary

This increment connects the reconstructed login/role flow to the strongest currently verified `GameLevels::LoadGL_Scene()` evidence without attempting to instantiate a `GameScene` prematurely.

## Trigger

The login callback router maps `cpp_OnEnterGame` to `ManagementRoute::kEnteringGame`. After callback-state locking is released, the runtime probes the user-imported resource:

`<files>/assets/gamescene/gs_list/pvp_scene.glData`

No scene rendering or gameplay is claimed at this point.

## Current verified boundary

The bounded parser now verifies:

1. top-level signed `int32` plus `scene_count`;
2. first-scene string/point/layer-count header;
3. first-layer float/object-count header;
4. first-object `int32` plus proven string byte length;
5. one skipped byte and exactly that many string bytes;
6. five floats structurally assigned as `CCPoint + float + CCPoint`;
7. one trailing `int32`;
8. two one-byte bool fields.

Parsing stops immediately after the second bool, before the following conditional object block. See `game-levels-first-object-core.md` for the ARMv7 range-construction evidence.

`kFirstObjectCoreVerified` means only that the runtime reached this strongest proven binary boundary. It does **not** mean a `GameScene`, layer object, player, renderer, physics system, or gameplay loop has been instantiated.

## State and diagnostics

The transition tracks callback/probe counts, source/reader byte sizes, and the number of bytes verified by the bounded parser. It never emits imported object strings or opaque numeric values through diagnostics.

The old 8-byte first-object prefix remains an intermediate parser milestone, but entering-game readiness now requires the complete verified first-object core.

## Next boundary

The next original code conditionally reads more uint32 data after the two bool fields. That conditional block must be established from ARMv7/Ghidra evidence before parsing advances further.
