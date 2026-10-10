# Development process

`docs/playable-path.md` is the primary delivery metric. `docs/roadmap.md` remains the strategic reverse-engineering roadmap, but day-to-day work should be chosen by distance to the next contiguous playable-path checkpoint.

## Work selection

1. Pick the earliest incomplete playable-path checkpoint.
2. Identify the smallest evidence-backed blocker to that checkpoint.
3. Assign it to exactly one lane (P1A-P1D).
4. Open one issue with a bounded definition of done.
5. Implement one vertical task and close the issue through a PR.
6. Record the next exact unknown before starting adjacent work.

## WIP limits

- one active implementation task per lane;
- at most two active tasks that directly modify the same contiguous playable-path transition;
- no speculative parser extension beyond a proven binary boundary;
- no duplicate agent work on the same issue without an explicit handoff.

## CI lanes

### Fast checks

`.github/workflows/ci-fast.yml` runs on pull requests and non-main branches. It covers inexpensive Python/tool validation plus critical startup/login/server/role/scene parser host smokes.

Goal: fail quickly before Android SDK/NDK work is spent.

### Full validation

`.github/workflows/ci-full.yml` runs on `main`, manually, and nightly. It preserves the broader host/Lua validation, builds the Android APK, checks 16 KiB compatibility and archives a commit-addressed debug artifact.

### Android emulator smoke

`.github/workflows/android-emulator-smoke.yml` runs manually and on a scheduled cadence against Android API 35 and 36. Changes to the workflow itself also run the matrix on the pull request.

Each x86_64 emulator job now exercises more than process launch:

- foreground launch and fatal-logcat gate;
- Home/pause while requiring the same process to remain alive;
- foreground resume while requiring the same PID;
- Back/Activity finish followed by immediate relaunch in the retained process, exercising a fresh `Activity`/`GLSurfaceView`/EGL-surface lifecycle;
- force-stop plus cold restart;
- process/foreground/fatal checks after each relevant phase.

Normal builds remain `arm64-v8a` + `armeabi-v7a`; x86_64 is enabled only with `-PciEmulator`.

Physical/emulated arm64 and real 16 KiB page-size runtime validation are still required release gates; the x86_64 emulator job does not substitute for them. The evidence matrix and exact distinction between CI transport and release proof are recorded in `docs/android-runtime-validation.md`.

## Pull requests

Use `.github/PULL_REQUEST_TEMPLATE.md`. A reconstruction PR must answer:

- what evidence supports this behavior;
- what clean-room behavior was implemented;
- what automated test protects it;
- what exact gap remains.

Prefer a PR that creates a new observable runtime state over multiple micro-PRs that only move internal structures.

## Reverse-engineering evidence

Use `docs/reverse-engineering-evidence-template.md` when a change depends on binary behavior, parser layout, random/timeline semantics, recovered call order, or original resource structure.

Confidence must be explicit. Low-confidence hypotheses are experiments, not compatibility contracts.

## User-owned resources

The extracted APK/OBB tree remains outside Git. Use `tools/resource_index.py` to create deterministic local metadata and `docs/resource-index.md` for the sharing policy.

Never commit original proprietary binaries or asset contents.

## Parallel agent assignment

- **P1A:** critical runtime integration and contiguous playable path.
- **P1B:** GameLevels/scene binary formats and models.
- **P1C:** player/enemy/combat/equipment behavior.
- **P1D:** resource tooling, CI, Android/platform validation, reproducibility.

An issue is the unit of ownership. Agents should select distinct issues rather than generic instructions such as "continue development" when parallel execution is used.
