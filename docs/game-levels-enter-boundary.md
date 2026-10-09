# GameLevels enter-game boundary

This increment connects the reconstructed login/role flow to the existing bounded `GameLevels::LoadGL_Scene()` evidence without extending the binary parser beyond proven fields.

## Trigger

The recovered login callback router already maps `cpp_OnEnterGame` to `ManagementRoute::kEnteringGame`. After that callback has been captured and the callback-state mutex released, the runtime now probes the imported user-owned scene resource:

`<files>/assets/gamescene/gs_list/pvp_scene.glData`

No scene loading is claimed at this point. The transition records only whether the resource transport and the currently verified parser prefix are available.

## Verified boundary

The strongest current `LoadGL_Scene()` evidence reaches:

1. the top-level signed `int32` plus `scene_count`;
2. the first scene header through `layer_count`;
3. the first layer header through `object_count`;
4. the first object's immediately sequential `int32` / `uint32` prefix.

Parsing stops there, before the following unresolved-width `char*` read. `game_levels_enter_transition` consumes only the existing `game_levels_asset_probe::Snapshot`; it does not add fields to the parser or assign semantics to opaque values.

`kFirstObjectPrefixVerified` therefore means only that the runtime has reached the strongest currently proven binary boundary and is ready for the next clean-room reconstruction step. It does **not** mean that a `GameScene`, layer, object, player, renderer, or gameplay loop has been instantiated.

## State and diagnostics

The transition tracks:

- number of `cpp_OnEnterGame` boundary observations;
- number of bounded scene-probe attempts;
- scene file/reader size;
- the number of bytes verified by the first-object-prefix parser;
- a conservative boundary classification for missing/rejected/incomplete resources.

The status is exposed in both bootstrap diagnostics and the entering-game client UI report. No parsed proprietary string or opaque numeric field values are emitted.

## Tests

The host smoke covers missing/unconfigured/rejected/incomplete probe states, a fully verified synthetic prefix, reset semantics, and the production imported path under `assets/gamescene/gs_list/pvp_scene.glData`.

The next parser extension must be backed by new ARMv7/decompiler evidence for the field immediately after the current object prefix. Until then, this boundary must remain fixed.
