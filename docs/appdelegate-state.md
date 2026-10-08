# Reconstructed AppDelegate startup state

This document describes the project-owned startup/lifecycle state that now sits between the modern Android shell and the recovered splash/login flow. It is intentionally a semantic reconstruction rather than an ABI-compatible reimplementation of the original Cocos2d-x `AppDelegate`.

## Purpose

The Android shell already had independent pieces for:

- runtime configuration;
- GLSurfaceView lifecycle;
- the fixed-step 35 Hz game clock;
- recovered splash timing;
- SingleLogin rendering;
- tap-to-start and offline role routing.

`app_delegate_state.{h,cpp}` gives those pieces one host-testable startup state contract so diagnostics and later scene work can reason about a single lifecycle instead of inferring state from unrelated counters.

## Phases

The state machine exposes these phases:

1. `cold` — no runtime configuration has been supplied;
2. `runtime-configured` — app-private paths/device/version are available;
3. `surface-ready` — the recovered visual route has reached its first usable GL surface;
4. `foreground-ready` — the Activity is resumed and the surface is ready, before the recovered splash begins;
5. `splash-running` — the six-frame recovered splash sequence is active;
6. `initial-ui-ready` — the recovered splash timeline has completed and the initial login/UI route may render;
7. `background` — Activity lifecycle is paused regardless of the previous foreground phase.

`initial-ui-ready` is deliberately a readiness boundary. It does **not** claim that the original `ManagementLayer` implementation has been fully reconstructed.

## Android integration

`MainActivity` forwards pause/resume both to the existing render/game-clock lifecycle and to the project-owned AppDelegate state. Runtime configuration marks the state as configured.

The recovered scene bridge marks the surface ready on the first visual-sequence reset, records splash generation/start tick when the sequence begins, and feeds splash completion into the state during the normal per-frame SingleLogin activity check.

Repeated asset reloads do not manufacture new surface generations; surface readiness is idempotent until a future explicit context-generation hook is added.

## Diagnostics

`nativeBootstrapInfo()` now includes an `app delegate state` block with:

- current phase;
- runtime/surface readiness;
- pause/resume counters;
- recovered scene generation;
- splash start/current ticks.

This makes Android/device testing able to distinguish lifecycle failures from rendering, asset import or Lua/startup failures.

## Regression coverage

`tools/app_delegate_state_smoke.cpp` covers the expected semantic progression:

```text
cold
  -> runtime-configured
  -> surface-ready
  -> foreground-ready
  -> splash-running
  -> background
  -> initial-ui-ready after resume
```

The test also verifies generation/tick accounting and the public diagnostic report.

## Next boundary

The next reconstruction step should consume `initial-ui-ready` as an explicit transition into the reconstructed `HelloWorld::createUI()` / `ManagementLayer::initLoginLayer()` behavior instead of letting the renderer infer the transition only from splash timing.
