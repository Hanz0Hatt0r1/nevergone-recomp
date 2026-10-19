# Retained GameLevels runtime state

`game_levels_runtime_state` is the project-owned retained runtime layer above the recovered GameLevels binary framing. It loads the user-owned scene file into `game_levels_model::Model` once, keeps the current port/scene selection alive across navigation steps, and materializes the current scene into the evidence-backed construction/render representations.

## Loading

The runtime follows the reconstructed enter-game path at `<files_dir>/assets/gamescene/gs_list/pvp_scene.glData`. Files larger than 64 MiB are rejected before allocation. The model parser retains the exact verified loader end offset and does not require physical EOF equality, matching the recovered original `LoadGameLevels()` behavior.

A failed load clears any previously retained model, scene instance, construction plan and render queue so stale scene state cannot survive a failed EnterGame attempt.

`client_callback_bridge` invokes `game_levels_enter_transition::on_enter_game(files_dir)` from the real `cpp_OnEnterGame` callback. Once the probe reaches the verified PortNode section, the transition calls `load_pvp_scene()` and exposes the strongest resulting runtime boundary.

## Current scene state

A ready snapshot exposes only clean-room/project-owned values: current port index, current scene index/GUID, stored numeric event-port type, selected scene layer/object counts, construction-plan counts, and render-queue counts. The original pointer graph is never retained.

`step()` applies the recovered `GetPortNodeLinkPortNode()` semantics and immediately refreshes the current scene through the proven `current_port.first_string -> SceneRecord.string_value` mapping.

For every current scene the runtime attempts to retain:

1. `game_levels_scene_instance::SceneInstance` — the selected parsed scene expressed as project-owned values;
2. `game_scene_construction_plan::ScenePlan` — recovered layer/object traversal plus only proven object-construction semantics;
3. `game_scene_render_queue::Queue` — currently renderable object/resource requests in preserved traversal order.

The EnterGame transition consequently distinguishes `runtime-model-ready`, `runtime-scene-instance-ready`, `runtime-scene-construction-plan-ready`, and `runtime-render-queue-ready` rather than stopping at a parser-only diagnostic.

## Raw `LoadGL_Global` enemy diagnostics

The final recovered array inside `HPData::LoadGL_Global` contains fixed 32-byte records in this exact physical order:

```text
i32, i32, f32, i32, i32, i32, i32, i32
```

Original consumer evidence now identifies the tutorial-relevant fields:
`first_i32` supplies `enemy_id`, `second_i32` supplies base health, and
`first_float` supplies the attack input stored at `EnemyObject +0x2bc` after
`VCVT.U32.F32`. The other five integers retain structural names because their
gameplay semantics are not yet proven.

`game_levels_runtime_state` therefore exposes the parsed records without renaming fields:

- `Snapshot::global_enemy_record_count` retains only the number of records;
- `current_global_enemy_records()` returns a copy of the parser-owned records;
- `status_report()` prints at most the first 16 records in exact on-disk field order as `global enemy[N]: ...`;
- when more than 16 records exist, the report emits only the omitted count;
- a failed load clears the count and makes the accessor empty, so diagnostics cannot accidentally describe a previous scene.

The runtime diagnostic continues to expose the unmodified parsed record for
inspection. The reviewed enemy mapping and combat-input resolver live in
`game_levels_enemy_record_mapping`; the tutorial Attack path uses them to
materialize health and retain the converted combat input in actor bindings.

No proprietary scene bytes are stored by this diagnostic; values appear only in the local runtime evidence report produced from user-imported assets.

## Unresolved object-type diagnostics

`GameSceneLayerObjectData + 0x14` is already proven to be the original object type code. The project still assigns construction semantics only where evidence exists, so a real scene can legitimately contain `ObjectConstructionKind::kUnresolved` entries.

`game_scene_construction_diagnostics` summarizes those entries without guessing behavior or exposing resource names/paths:

- counts only construction-plan objects whose kind is `kUnresolved`;
- groups by the already proven signed `type_code`;
- sorts codes deterministically;
- reports at most 16 distinct codes and records how many additional categories were omitted;
- retains both the distinct-type count and a bounded histogram such as `2=1,6=3` in the runtime snapshot.

The normal runtime report now includes this histogram when unresolved construction types are present. This makes a real-device P7 run actionable: a scene that is visually incomplete can identify the exact type-code families that still need evidence, without publishing filenames or proprietary scene contents.

This histogram is diagnostic evidence only. Seeing type code `6`, for example, does not grant project-owned semantics to that code; its original pre-construction branch still depends on unresolved game/player state.

## Regression coverage

`tools/game_levels_runtime_state_smoke.cpp` builds a fully synthetic fixture, loads it through the same app-private `pvp_scene.glData` path, verifies all retained boundaries, traverses the synthetic PortNode graph, verifies stable state on an unsupported event, then exercises size-limit and missing-file failures.

The fixture now also contains two synthetic global enemy records with deliberately distinctive signed integer and float values. The smoke verifies exact field preservation, bounded report formatting, retention across scene navigation, and revocation after later failed loads. These values are test sentinels only and do not claim any original-game field semantics.

`tools/game_scene_construction_diagnostics_smoke.cpp` verifies deterministic grouping, exclusion of already resolved type-0 sprite/action objects, signed type-code handling, bounded reporting after 16 distinct categories, and the empty-scene case. No proprietary game data is committed.

The runtime smoke also verifies that construction and render state are revoked when a later load fails.

## Next boundary

The tutorial enemy roster is now linked into the Android runtime, but materialization deliberately waits for real imported base health. The immediate evidence boundary is therefore the semantic mapping of the raw `LoadGL_Global` enemy record: establish the enemy identity field and the base-health field from an original consumer or equivalent high-confidence runtime evidence, then provide those accessors to `tutorial_enemy_roster`.

Other downstream work remains:

- nonzero `GameSceneObject` construction remains semantically unresolved; real-device histograms can identify which exact codes matter first;
- type `6` pre-construction behavior still requires its game-mode/player-state consumer evidence;
- type-0 scene-action-pair playback remains deferred;
- live EnemyObject damage/death wiring remains after the imported health mapping;
- P7 still requires a real Android visual proof against user-imported original resources before it is considered complete.

The runtime must not manufacture those semantics from plausible bytes. New fields should be exposed only when an original consumer or equivalent high-confidence evidence establishes their meaning.
