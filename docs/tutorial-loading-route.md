# Fresh-role tutorial loading boundary

This note records clean-room evidence for the loading presentation between fresh-role creation and the first tutorial scene. Original screenshots, heap dumps, disassembly and proprietary image bytes are not stored in the repository.

## Frozen runtime capture

The supplied scene-state capture was taken with the original Android process stopped by `SIGSTOP` for the complete `/proc/<pid>/maps`, `smaps`, screenshot and memory-dump interval. It therefore represents one consistent loading state rather than a mixture of frames while the game continued running.

The captured frame shows a dedicated black loading presentation after character creation and before the tutorial overlay. The original process memory contains the loading-tip text `You can jump farther while you are running` and the tutorial resource name `cp_lesson.glData` in this state.

The original native binary also retains the Cocos search path `gamescene_ui/loading_UI` and the resource names `loading_UI/logo_01.png`, `loading_UI/logo_02.png`, and `loading_UI/logo_03.png`. These names are used only to resolve user-imported original resources at runtime; no image bytes are committed.

A later frozen capture shows `LessonLayer` after loading. Together the captures establish this ordering for the fresh-role path:

```text
SingleSelectHero / CharacterName
        -> LoadingLayer
        -> cp_lesson.glData
        -> GameScene / LessonLayer
```

## Reconstructed route

`offline_startup_flow::on_fresh_role_local_enter()` no longer collapses the preservation path directly into `kGameLevels`. Once the recovered CreateCharacter dispatch succeeds and imported GameLevels reaches `runtime-render-queue-ready`, it enters `kTutorialLoading`.

The loading route is deliberately separate from the already-prepared GameLevels state. `tutorial_loading_compositor` owns the full surface while this route is active, clears it to the captured black background, and renders imported `logo_01..03.png` frames when available.

`TutorialLoadingAssetLoader` reads those files only from the user's imported asset tree and uploads decoded pixels to GLES. If the frames cannot be resolved, the black loading boundary remains visible instead of leaking already-prepared gameplay through it.

## cp_lesson bootstrap

`tutorial_scene_bootstrap` now connects the proven loading boundary to the existing project-owned GameLevels parser without inventing a duration.

The bootstrap is armed only while the fresh-role route is `tutorial-loading`. It first requires at least one frame in which the loading compositor actually owned the surface. Only after that presentation has happened does it resolve the evidence-backed basename `cp_lesson.glData` from the user's imported asset tree.

The exact original directory for `cp_lesson.glData` is not yet proven. The resolver therefore:

1. checks the conventional imported GameLevels location `assets/gamescene/gs_list/cp_lesson.glData`;
2. if absent, performs a bounded basename search below `assets/` with a maximum depth of 16 and at most 8192 inspected entries;
3. fails closed if no match exists, if the fallback finds more than one match, or if the search bound is exhausted.

A resolved file is first checked through the same complete Scene -> Actions -> Global -> PortNode probe used by other imported GameLevels evidence. It is then loaded through `game_levels_runtime_state::load_file()`, which replaces the previously prepared preservation scene with the tutorial GameLevels model and rebuilds the scene instance, construction plan and render queue.

The loading compositor remains authoritative while Java/native texture staging catches up with the new queue. The bootstrap does not complete merely because parsing succeeded. It waits until `game_scene_direct_renderer::draw()` reports a successful frame while the retained runtime still matches the exact `cp_lesson` load attempt and selected scene. This is the concrete runtime completion event.

Only then does the coordinator advance both retained state machines:

```text
tutorial_session_state: Loading -> LessonOverlay
offline_startup_flow:   tutorial-loading -> game-levels
```

The transition is deliberately observed near the end of the frame, after base-scene/Controls decisions were made using the old loading route. Consequently the first exposed tutorial-scene frame cannot accidentally acquire Controls geometry that was hidden behind LoadingLayer in the same frame.

## Failure diagnostics

The real-device diagnostics now append a `Tutorial scene bootstrap` block to the existing GameScene asset-staging report. Relevant states include:

```text
waiting-loading-presentation
scene-missing
scene-ambiguous
search-limit-reached
scene-rejected
runtime-load-failed
runtime-queue-unready
waiting-scene-frame
runtime-changed
complete
```

The block also records the resolver mode, resource-relative scene path, verified GameLevels byte count, retained runtime load attempt, selected scene GUID, render-sprite count, observed scene-frame attempts and completion count. Absolute app-private paths are not reported.

The adjacent `Tutorial session` diagnostics identify whether the lifecycle is still `Loading` or has advanced to `LessonOverlay`.

## Completion boundary

`offline_startup_flow::on_tutorial_loading_complete()` remains explicit and is still **not** driven by a fabricated timer. The frozen evidence proves the ordering but no fixed loading duration.

The current runtime completion rule is therefore: a real loading presentation must have been shown, the user-imported `cp_lesson.glData` must parse into a live GameLevels render queue, and that exact retained queue must produce a successful project-owned GameScene frame. If any of those gates fails, the runtime remains on LoadingLayer and reports the exact failed boundary instead of skipping forward.

The next fidelity boundary after this handoff is the visible `LessonLayer` overlay/content itself and its explicit completion event into the already-modeled post-lesson Dialogue phase.
