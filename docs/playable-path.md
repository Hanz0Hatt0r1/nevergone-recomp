# Playable path

This document is the primary delivery metric for the Never Gone recompilation.

The project should optimize for the shortest evidence-backed path from a clean launch to an observable gameplay state, not for the raw number of reconstructed subsystems.

## Target vertical slice

```text
cold start
  -> import user-owned original data
  -> reconstructed startup/login
  -> server selection
  -> ChooseHero
  -> enter-game transition
  -> GameLevels scene load
  -> scene instantiated
  -> player spawned
  -> player input visible on screen
  -> minimal save
  -> process restart
  -> minimal restore
```

A checkpoint counts as complete only when every preceding checkpoint is also complete in the same project-owned runtime path. Disconnected downstream probes and host regressions are useful implementation evidence, but they do not advance the contiguous real-device completion marker by themselves.

## Checkpoints

| ID | Checkpoint | Definition of done | Current state |
| --- | --- | --- | --- |
| P0 | Clean launch | Project-owned APK starts without the original native library. | **Done** |
| P1 | Original data available | User-owned APK/OBB import is available and runtime search paths can find imported data. | **Done** |
| P2 | Startup/login | Reconstructed AppDelegate/Lua startup reaches the login/management path. | **Done** |
| P3 | Server selection | Server list renders, accepts selection and confirmation, and emits exactly one project-owned enter request. | **Done** — tracked issue #144 closed |
| P4 | ChooseHero | Role data renders, selection/focus/profile state is usable, and role choice reaches reconstructed login Lua. | **Done** for the tracked existing-role route — issue #145 closed |
| P5 | Enter-game transition | Confirmed role selection deterministically enters the next offline route without original native code. | **Done** for the tracked existing-role route — issue #145 closed |
| P6 | GameLevels load | Required scene data is found and parsed through all fields needed by the first offline scene-construction boundary. | **Done** — tracked issue #146 closed |
| P7 | Scene instantiated | A project-owned scene graph is built and rendered from imported original data. | **Done for the fresh-role tutorial route** — integrated Redmi Note 8 Pro capture on 2026-10-16 passed with 9 staged requests, 9 drawn sprites and no hidden slots; see `docs/evidence/player-clothes-frame-lookup-2026-10-16.md` |
| P8 | Player spawned | Player entity/visual state appears at an evidence-backed spawn point. | **Done** — the player body resolves and renders at `(430, 271)` in the contiguous fresh-role scene; see `docs/evidence/player-clothes-frame-lookup-2026-10-16.md` |
| P9 | Input visible | User input changes player state visibly in the first offline scene. | **Done** — an Android joystick gesture visibly moved the player right in that scene; device diagnostics recorded 2 accepted begins and 73 moves; see `docs/evidence/player-clothes-frame-lookup-2026-10-16.md` |
| P10 | Minimal persistence | Save -> process restart -> restore works for the minimal vertical slice. | **Done** — the P7–P10 contiguous device harness passed; a non-zero player motion offset survived a process restart and matched the restored runtime state |

The highest contiguous fresh-role checkpoint is P10 on the tested Android device.

## Current critical path

The implementation bottleneck has moved beyond startup and core rendering infrastructure. The current critical path is:

1. make the fresh-role tutorial scene capture repeatable and align player spawn/camera plus HUD/instruction state against the original-game device capture;
2. validate the structural EnemyActions/TrapActions work against genuine user-owned data and connect the minimum first combat interaction;
3. retain API 35/36 arm64 physical-device evidence.

SceneActions work may proceed in parallel where it directly improves the first playable scene, but unrecovered spatial trigger and global PRNG behavior must remain explicit evidence boundaries rather than guessed runtime logic.

## Parallel work lanes

Parallel agents/processes should take non-overlapping lanes.

| Lane | Scope | Typical directories/data | Blocking output |
| --- | --- | --- | --- |
| P1A - critical runtime | First-scene integration, SceneActions, contiguous P7-P9 route | `android/`, scene runtime, renderer/input | advances P7-P9 |
| P1B - scene formats | `GameLevels`, `HPData`, scene/action/state-machine formats | `gamescene/*`, parsers | evidence/models required by scene runtime |
| P1C - player/combat | player, enemy, NPC, weapons/equipment behavior | `player`, `enemy`, `NPC`, `weapons`, `equips` | P8/P9/combat models |
| P1D - tooling/platform | resource indexing, Android CI/device checks, evidence tooling | `tools/`, `.github/`, Android platform | keeps all lanes reproducible |

A lane should not take work already active in another lane unless explicitly handing it over.

## WIP rule

Keep at most one active implementation task per lane and at most two active tasks on the contiguous playable path. Finish or close a blocking gap before opening another speculative reconstruction branch.

## Pull request contract

Every reconstruction PR should contain four sections:

1. **Evidence** - original symbol/address/resource/control flow or other evidence that justifies the behavior;
2. **Implementation** - the project-owned behavior added or changed;
3. **Tests** - synthetic/host/device coverage that can run without proprietary data where possible;
4. **Remaining gap** - the exact next unknown boundary, rather than a broad subsystem wish list.

Prefer a complete vertical task over many micro-PRs that do not create a new observable state.

## Resource policy

Original APK/OBB files and extracted proprietary assets remain outside this repository. Local indexes generated by project tools may be used to coordinate work. Share aggregate summaries or hashed-path indexes where possible; do not commit original file contents.

## Alpha gates

### Technical alpha

Reached when P0-P9 pass in one real Android runtime path using user-owned imported data.

### Early playable alpha

Technical alpha plus:

- at least one enemy interaction;
- damage/death loop;
- minimal save/restore proven through process restart;
- no blocking crash in the first gameplay slice.

### Release-readiness gate

Requires the playable path plus Android 15/16 validation, arm64 runtime validation, 16 KiB runtime validation, lifecycle/recreation coverage, and a reproducible release build.
