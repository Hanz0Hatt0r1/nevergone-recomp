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

## Completion boundary

`offline_startup_flow::on_tutorial_loading_complete()` is explicit. It is **not** currently driven by a fabricated duration. The frozen evidence proves that a loading boundary exists, but does not prove a fixed loading time.

The next reconstruction step is to connect this completion event to the evidence-backed `cp_lesson` / `LessonLayer` bootstrap. Until that event is reconstructed, the preservation runtime intentionally stops on the loading presentation instead of skipping a proven original state.
