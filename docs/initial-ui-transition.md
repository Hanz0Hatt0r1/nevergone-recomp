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

## ManagementLayer login routes

The same state now models the first callback-driven `ManagementLayer` UI routes plus the project-owned wait boundary after a successful server-enter dispatch:

```text
login-root
  -> announcement       cpp_OnGameAnnoucement
  -> server-selection   cpp_OnGetServerList
       -> awaiting-role-list   successful EnterGameLogicServer dispatch
            -> role-selection  cpp_OnGetRoleList
  -> role-created       cpp_OnCreateTheRole
  -> entering-game      cpp_OnEnterGame
```

These names describe reconstructed semantic routes, not original class layouts or widget implementations. `awaiting-role-list` is intentionally not presented as an original callback route: it records that the recovered server-enter request completed locally while ownership of the next visible role route remains with `cpp_OnGetRoleList`.

Relevant callbacks that arrive before `management-login-initialized` are retained as a pending route. They do not become visible to the renderer until the verified `HelloWorld::createUI()` / `ManagementLayer::initLoginLayer()` boundary is reached. Once initialized, subsequent relevant callbacks move the reconstructed UI route in arrival order.

A successful `EnterGameLogicServer` dispatch may move `server-selection` to `awaiting-role-list`. Failed dispatches do not advance the route, and repeated success notifications outside `server-selection` are ignored. The project does not synthesize `cpp_OnGetRoleList`; a real captured callback is still required to reach `role-selection`.

Diagnostic callbacks such as chat, update-data and PVE-connect remain captured but do not change the login route.

`client_callback_bridge.cpp` keeps the original captured payloads but projects the renderer-visible login payloads through the active Management route. This removes the previous behavior where the renderer inferred a route solely from whichever stored callback string happened to be non-empty.

## Generation behavior

The state is keyed to the recovered splash scene generation. Importing/reloading assets or beginning a new recovered scene sequence resets both the startup transition and the Management login route. A stale ready signal or pending callback from an earlier generation cannot activate the new login scene.

Repeated polling after initialization is idempotent and does not replay `createUI`/`initLoginLayer` semantics.

## Renderer integration

`single_login_scene_bridge.cpp` updates `AppDelegateState`, synchronizes `initial_ui_transition`, and only reports the reconstructed SingleLogin/SingleSelectHero route active after the phase reaches `management-login-initialized`.

The same boundary also gates `splash_sequence_state::single_login_seconds()`. This keeps native SingleLogin rendering, thunder/audio timing and Java-visible route state aligned: completion of the logo timeline alone is no longer enough to advance SingleLogin local time.

On the first frame where the splash completes, the compositor remains inactive until the bridge records the recovered HelloWorld/ManagementLayer transition. Subsequent frames then use the same scene generation and local login time. A hot asset reload revokes this gate for the new generation.

The existing `offline_startup_flow` remains responsible for local standalone-role routing. The Management route state covers the callback-driven login/server/role path and provides a stable place to attach recovered widgets as their behavior is reconstructed.

## Scope

This does **not** claim that all behavior inside `HelloWorld::createUI()` or `ManagementLayer::initLoginLayer()` has been reconstructed. It establishes the verified startup boundary plus the first callback-driven UI routing semantics so later work can replace diagnostic rendering with recovered widgets without changing the route contract.

The live gap between `awaiting-role-list` and ChooseHero/role selection is the service-provided `cpp_OnGetRoleList` payload. That dependency and the non-synthetic policy are detailed in `docs/server-enter-route-transition.md`.

Host regression coverage lives in `tools/initial_ui_transition_smoke.cpp` and `tools/splash_sequence_state_smoke.cpp`.
