# Initial UI transition reconstruction

This document records the clean-room semantic boundary that connects the reconstructed `AppDelegate` startup state to the first recovered login UI.

## Original evidence

The recovered native startup flow establishes this synchronous sequence after the logo/splash action chain:

```text
HelloWorld::createUI()
        |
        v
ManagementLayer::initLoginLayer()
```

The project does not recreate the original C++ class ABI. Instead, `initial_ui_transition` records when the verified boundary becomes eligible and exposes that state to the modern renderer.

## Reconstructed phases

```text
waiting-for-appdelegate
        |
        | AppDelegateState == initial-ui-ready
        v
hello-world-create-ui
        |
        | verified synchronous native transition
        v
management-login-initialized
```

`hello-world-create-ui` is retained as an explicit semantic step even though the recovered call into `ManagementLayer::initLoginLayer()` is synchronous. The transition counter therefore advances twice when a scene generation first reaches the initial-UI boundary.

## Generation behavior

The state is keyed to the recovered splash scene generation. Importing/reloading assets or beginning a new recovered scene sequence resets the transition. A stale ready signal from an earlier generation cannot activate the new login scene.

Repeated polling after initialization is idempotent and does not replay `createUI`/`initLoginLayer` semantics.

## Renderer integration

`single_login_scene_bridge.cpp` updates `AppDelegateState`, synchronizes `initial_ui_transition`, and only reports the reconstructed SingleLogin/SingleSelectHero route active after the phase reaches `management-login-initialized`.

The same boundary also gates `splash_sequence_state::single_login_seconds()`. This keeps native SingleLogin rendering, thunder/audio timing and Java-visible route state aligned: completion of the logo timeline alone is no longer enough to advance SingleLogin local time.

On the first frame where the splash completes, the compositor remains inactive until the bridge records the recovered HelloWorld/ManagementLayer transition. Subsequent frames then use the same scene generation and local login time. A hot asset reload revokes this gate for the new generation.

The existing `offline_startup_flow` remains responsible for post-login local routing such as the recovered standalone-role branch. This change only establishes the verified UI initialization boundary before those routes are allowed to render.

## Scope

This does **not** claim that all behavior inside `HelloWorld::createUI()` or `ManagementLayer::initLoginLayer()` has been reconstructed. It provides a stable, testable boundary for incrementally adding the recovered login widgets, callbacks and state in the correct startup order.

Host regression coverage lives in `tools/initial_ui_transition_smoke.cpp` and `tools/splash_sequence_state_smoke.cpp`.
