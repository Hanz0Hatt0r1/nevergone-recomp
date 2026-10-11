# First GameScene runtime render evidence

P7 is a real-device proof boundary. Synthetic fixtures establish parser, staging and renderer contracts, but they do not prove that user-owned imported Nevergone data reaches the complete `GameLevels -> render queue -> pixel staging -> GLES draw` path on Android.

`tools/android_game_scene_render_evidence.sh` captures that proof without copying proprietary assets into the repository.

## Preconditions

1. Install the current Nevergone recomp APK on an Android device.
2. Import the user's original APK and OBB through the app's import controls.
3. Follow the reconstructed offline route through server selection and ChooseHero until the first GameLevels scene is active.
4. Leave Never Gone Recomp in the foreground with the first scene visible.
5. Connect exactly one adb device, or set `ANDROID_SERIAL` explicitly.

The capture harness does not restart the Activity or force the gameplay route. If Nevergone is not the resumed Activity, it fails rather than destroying the scene state it is intended to prove.

## Capture

From the repository root:

```bash
bash tools/android_game_scene_render_evidence.sh
```

The default output directory is:

```text
artifacts/android-game-scene-runtime/
```

Override it with `NEVERGONE_EVIDENCE_DIR` when needed.

The harness searches the existing diagnostics `ScrollView` in both directions, presses **Refresh runtime diagnostics**, extracts the single resource-name-free GameScene diagnostics node, and captures the current Android screen. It retains:

- `runtime-report.txt` — device/runtime metadata and SHA-256 bindings;
- `runtime-diagnostics.txt` — the refreshed Nevergone diagnostics text;
- `scene.png` — the actual on-device screen at capture time;
- `runtime-window.xml` — the final uiautomator snapshot used to extract diagnostics.

The adb serial and build fingerprint are not written to the report.

## What counts as a passing P7 render proof

`tools/android_game_scene_runtime_evidence.py validate runtime-report.txt` fails closed unless all of the following are true:

- original APK and OBB assets are reported present;
- `GameLevels enter boundary` is `runtime-render-queue-ready`;
- the runtime model, scene instance, construction plan and render queue are all ready;
- the base GameLevels render queue contains at least one sprite;
- `GameScene asset staging` is `ready`, has at least one successful staging attempt, a nonzero live revision, a nonempty request set and `staged == requests`;
- `GameScene static renderer` has a ready shader and a nonzero surface;
- at least one draw attempt and one successful drawn frame are recorded;
- the last drawn static sprite count is nonzero and matches the completely staged request count;
- both retained evidence files match the SHA-256 values in the report;
- the screen capture is a PNG with nonzero dimensions.

`render sprites` is the base queue produced by the loaded `GameLevels` scene. Before the direct renderer creates asset requests it composes project-owned live overlays through `game_scene_player_render_queue`, including the recovered joystick dock and, when available, the player frame. Therefore the staged/drawn count may legitimately exceed the base `render sprites` count. The fail-closed post-composition invariant is `last static sprites == requests == staged`, not `last static sprites <= render sprites`.

The validator deliberately does **not** require `deferred scene-action pairs` or `unresolved render objects` to be zero. P7 only requires the evidence-backed static scene/background layer to render safely. Those counters are surfaced in the successful validator summary so the next blocker to full scene fidelity and player spawn remains explicit.

## Failure evidence

If the live state does not satisfy the proof, the harness rewrites `runtime-report.txt` with `status=fail` and preserves the diagnostics and screenshot. This is diagnostic evidence, not a completed P7 proof.

A failed report can still be checked for report/file integrity with:

```bash
python3 tools/android_game_scene_runtime_evidence.py validate \
  artifacts/android-game-scene-runtime/runtime-report.txt \
  --allow-failure
```

Do not commit captures containing user-owned runtime data. Commit only code, synthetic tests and summary conclusions derived from the evidence.

## Relation to issue #147

A passing report proves the technical static-render boundary and produces the visual artifact needed for review. It does not by itself assert that every dynamic/animated GameScene object has been reconstructed. Record the retained `unresolved render objects` and `deferred scene-action pairs` values when closing P7; those values define the next implementation boundary before player creation/spawn.
