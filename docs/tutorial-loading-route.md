# Fresh-role tutorial loading boundary

This note records clean-room evidence for the loading presentation between fresh-role creation and the first tutorial scene. Original screenshots, heap dumps, disassembly and proprietary image bytes are not stored in the repository.

## Frozen runtime capture

The supplied scene-state capture was taken with the original Android process stopped by `SIGSTOP` for the complete `/proc/<pid>/maps`, `smaps`, screenshot and memory-dump interval. It therefore represents one consistent loading state rather than a mixture of frames while the game continued running.

The captured frame shows a dedicated black loading presentation after character creation and before the tutorial overlay. The original process memory contains the loading-tip text `You can jump farther while you are running` and the tutorial resource name `cp_lesson.glData` in this state.

The original native binary also retains the Cocos search path `gamescene_ui/loading_UI` and the resource names `loading_UI/logo_01.png`, `loading_UI/logo_02.png`, and `loading_UI/logo_03.png`. These names are used only to resolve user-imported original resources at runtime; no image bytes are committed.

A later frozen capture shows `LessonLayer` after loading. Together the captures establish this ordering for the fresh-role path:

```text
SingleSelectHero / CharacterName
        -> cp_lesson.glData load
        -> LoadingLayer
        -> GameScene / LessonLayer
```

## Original fresh-role call chain

The original ARMv7 library now closes the previously missing handoff.

`ManagementLayer::GoToGameLayer(int,bool)` has a dedicated tutorial branch. That branch configures the game mode as `0x0b`, passes the bare filename:

```text
cp_lesson.glData
```

to `GameScene::LoadGameLevelsWithFile()`, and only after that call returns invokes `GameScene::switchScene()`.

`GameLevels::LoadGameLevels(char const*)` does not prepend a hard-coded directory. It calls the ordinary `CCFileUtils` full-path resolver with the supplied bare filename. The existing `AppDelegate::AddAllSearchPath()` reconstruction proves the relevant search-path order: the imported `assets` root first, followed by the 59 recovered child directories including `gamescene/gs_list`.

The recomp therefore resolves `cp_lesson.glData` through the already shared `lua_runtime::resolve_resource_path()` contract instead of guessing a directory or recursively scanning the imported tree.

## Original LoadingLayer callback

`GameScene::switchScene()` enters `switchSceneFadeOut()`. The latter calls:

```text
GameSceneUI::showLoadingLayer(...)
```

with `GameScene` as the callback target. The retained member-function pointer resolves to:

```text
GameScene::switchNextScene()
```

`showLoadingLayer()` creates a `CCDelayTime` with the literal IEEE-754 value `0.5f`, followed by the callback action. The half-second boundary is therefore original-derived behavior, not an invented timeout.

The clean-room runtime uses its existing fixed 35 Hz `game_clock`. The first logical callback point at or after 0.5 seconds is 18 ticks, so the reconstructed boundary is:

```text
LoadingLayer begins
        -> >= 18 fixed game ticks (first tick not earlier than 0.5 s)
        -> switchNextScene readiness gate
```

At least one actual LoadingLayer presentation is also required before the route may advance, so a stalled/hidden surface cannot satisfy the transition solely by elapsed ticks.

## switchNextScene and LessonLayer

For tutorial mode `0x0b`, `GameScene::switchNextScene()` follows the same player/data setup branch as the normal mode-4 path, resets/removes the previous scene objects, and calls `GameScene::initGameScene()`.

The subsequent `GameScene::tick(float)` has the concrete readiness gate that was missing from the earlier reconstruction. Before entering the nonzero game-mode UI path it requires both:

```text
NPCManager::isInitALLNPCDone() == true
BattleManager::isInitAllEnemyDone() == true
```

For mode `0x0b`, that path calls:

```text
GameSceneUI::showLesson()
```

`showLesson()` creates the original `Lesson` UI (`Lesson::createLessonUI(1,1)`). The same tick then checks whether LoadingLayer is still shown and calls:

```text
GameSceneUI::hideLoadingLayer(0.2f)
```

The `0.2f` fade duration is also a literal recovered from the shipped binary.

The clean-room runtime does not yet own complete original NPC/enemy manager initialization. Its conservative readiness substitute is therefore a successful frame from the exact retained `cp_lesson` GameLevels/render queue after the original 0.5-second callback has become due. That frame proves that the replacement runtime, scene instance, construction plan, pixel staging and renderer have all caught up to the tutorial scene before LoadingLayer is retired.

## Reconstructed route

The production fresh-role path now follows the recovered ordering:

```text
CreateCharacter dispatch succeeds
        -> resolve cp_lesson.glData through original search paths
        -> complete GameLevels parse
        -> retained scene instance / construction plan / render queue ready
        -> offline_startup_flow::on_fresh_role_local_enter()
        -> tutorial_session_state::begin_loading()
        -> LoadingLayer presentation
        -> recovered 0.5 s callback boundary
        -> successful exact cp_lesson render frame
        -> tutorial_session_state: Loading -> LessonOverlay
        -> offline_startup_flow: tutorial-loading -> game-levels
```

`game_levels_enter_transition::on_local_enter_game_file()` carries the resolved `cp_lesson` file through the same fail-closed `GameLevels enter boundary` diagnostics used by real-device P7 validation. The previous fresh-role `pvp_scene.glData` preload is no longer used for this path.

## Loading presentation

`tutorial_loading_compositor` owns the full surface while the route is `tutorial-loading`, clears it to the captured black background, and renders imported `logo_01..03.png` frames when available.

`TutorialLoadingAssetLoader` reads those files only from the user's imported asset tree and uploads decoded pixels to GLES. If the frames cannot be resolved, the black loading boundary remains visible instead of leaking already-prepared gameplay through it.

The original 0.2-second visual fade is now proven but is not yet reproduced as alpha interpolation by the project compositor; the route transition currently retires the clean-room loading compositor after the exact scene-readiness handoff. That remaining visual timing difference is separate from the now reconstructed loading-completion control flow.

## Failure diagnostics

The real-device diagnostics append a `Tutorial scene bootstrap` block to the existing GameScene asset-staging report. Relevant states include:

```text
resolving-scene
files-dir-unconfigured
scene-missing
scene-rejected
runtime-load-failed
runtime-queue-unready
prepared
waiting-original-switch-delay
waiting-scene-frame
runtime-changed
complete
```

The block records:

- the original-resource search-path resolver mode;
- the resource-relative `cp_lesson` path selected by that resolver;
- verified GameLevels bytes;
- retained runtime load attempt and selected scene GUID;
- base render-sprite count;
- loading presentations;
- original `0.5 s` / 18-tick switch boundary;
- recovered `0.2 s` hide fade duration;
- successful scene-frame observations and completion count.

Absolute app-private paths are not reported.

The adjacent `Tutorial session` diagnostics identify whether the lifecycle is still `Loading` or has advanced to `LessonOverlay`.

## Next fidelity boundary

The loading-completion control flow is no longer intentionally open-ended. The next boundary is the visible `LessonLayer` content itself and its later explicit completion into the already modeled post-lesson Dialogue phase. The clean-room runtime also still needs a direct semantic reconstruction of the original NPC/enemy initialization readiness instead of using a complete rendered `cp_lesson` frame as the conservative substitute.
