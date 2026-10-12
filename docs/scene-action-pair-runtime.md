# SceneActions pair runtime staging

The first playable-scene construction path retains the exact `klhuo-1.png` object as a `SceneActionPairCommand`. The shipped branch consumes one process-global `lrand48()` result, maps it through the recovered float32 formula, and formats one born/idle pair:

- `se01_born%02d.actData`
- `se01_i%02d.actData`

## Runtime store

`scene_action_pair_document_store` is the project-owned native boundary for the two selected `TrapActionsData` documents. It keeps separate born and idle slots under one variant identity, confines relative paths beneath `filesDir/assets`, bounds file size, and parses each payload through `trap_actions_document`.

A pair becomes ready only when both slots for the same variant parse successfully. Changing variant invalidates the previous pair before admitting the new document. The store retains the parsed documents for later `SceneActionsSystem` playback reconstruction.

## Android staging

`SceneActionPairRuntimeLoader.stageVariant(assetRoot, variant)` resolves exactly one basename match for each file and sends their relative paths through JNI to the native store. Recursive discovery is bounded by file count and depth. Ambiguous basename matches fail closed.

The caller must supply a variant in the recovered range `1..4`. The loader does not:

- default to variant `01`;
- choose the first pair that happens to exist;
- infer the selected pair from missing imported files;
- fabricate a random value;
- start SceneActions playback or invent a playback timer.

This is intentional. Static evidence proves the one-draw variant formula and the original process-global `srand48(time(NULL))` seed, but the recompilation still models several earlier UI random consumers as independent local streams. Until their ordering is unified or a runtime capture establishes the actual draw reaching this `GameSceneObject::initWithData()` call, automatically choosing a pair would claim behavior that has not been recovered.

## Next boundary

The next integration can feed one evidence-backed 31-bit draw to `game_scene_type0_action_pair::build()`, pass its resulting variant to `SceneActionPairRuntimeLoader`, and then reconstruct the first `SceneActionsSystem` sprite/animation state from the retained born/idle documents.
