# ChooseHero random thunder scheduler

This note records the clean-room behavioral contract recovered from the shipped `ChooseHeroBackground::RamodThunder`, `FuncThunderBen`, and `FuncThunderEnd` path. No original code, binary bytes, or audio assets are stored here.

## RNG source

The shipped runtime uses the libc 48-bit random family rather than Cocos' higher-level random helpers. `AppParameters::init()` executes the equivalent of `srand48(time(nullptr))`, and the ChooseHero thunder path consumes the resulting process-global `lrand48()` stream.

The native code converts each non-negative `lrand48` result to a float with scale `2^-31`. The clean-room scheduler therefore exposes transformations from raw 31-bit values instead of inventing a separate distribution.

Exact cross-run random sequences are not a preservation requirement at this stage because other shipped subsystems also consume the same global stream before ChooseHero can be reached. The important recovered contract is the draw order and the transforms applied to each draw.

## Initial `RamodThunder()` schedule

`PartThree()` creates six invisible `menlei%02d.png` sprites and then calls `RamodThunder()`.

For each of the six thunder sprites, the shipped function:

1. calls `lrand48()` six times while repeatedly querying the thunder array count; those six results are overwritten and unused, but they still advance the global RNG;
2. consumes one random value for a delay: `3.0 + random * 5.0` seconds;
3. consumes one random value for fade-out duration: `random * 0.7` seconds;
4. stops existing actions on that thunder sprite;
5. runs `Delay(delay) -> FadeTo(0,255) -> FuncThunderBen -> FadeTo(duration,0) -> FuncThunderEnd`;
6. forwards the same `(delay,duration)` pair to child tag `101`, `ChooseHeroReadyUIScene::RandomShowwshandianEff`.

`choose_hero_thunder_scheduler::InitialRandomBatch` keeps the six otherwise-unused draws explicit so a later stateful runtime cannot accidentally alter the recovered RNG stream by dropping them.

## `FuncThunderBen()` sound selection

At the instant the thunder sprite becomes visible, `FuncThunderBen()` consumes one more `lrand48()` value and computes:

```text
index = trunc(abs(random - 0.01) * 30.0)
```

Only indices `0..6` play a sound. Larger values intentionally produce no thunder audio for that callback.

The seven recovered imported effects are:

1. `sound/SingleLogin_UI/L_thunder08-r.mp3`
2. `sound/SingleLogin_UI/L_Thunder09.mp3`
3. `sound/SingleLogin_UI/L_Thunder10.mp3`
4. `sound/SingleLogin_UI/M_thunder_norm_2.mp3`
5. `sound/SingleLogin_UI/M_thunder_norm_5.mp3`
6. `sound/SingleLogin_UI/M_Thunder04.mp3`
7. `sound/SingleLogin_UI/S_thunder_norm_1.mp3`

These are the same user-imported sound files already used by the reconstructed SingleLogin thunder audio path.

## `FuncThunderEnd()` reschedule

When a thunder sprite completes its fade-out, `FuncThunderEnd()` stops its actions and immediately schedules the next cycle.

It first consumes three random values to choose lightning indices from the six-element `shandian%02d.png` array using:

```text
index = trunc(abs(0.01 - random) * lightningCount)
```

The resulting six lookups are not six independent draws. They are:

```text
[A, B, C, C, C, C]
```

where only `A`, `B`, and `C` consume RNG values.

It then consumes two more values for the rescheduled thunder sprite:

```text
delay       = 3.0 + random * 5.0
fadeOutTime = random * 0.7
```

The thunder sprite receives the same action chain as the initial scheduler, with `FuncThunderBen` and `FuncThunderEnd` callbacks retained.

For each selected lightning sprite, when that sprite has no running actions, the function consumes a random value and uses:

```text
lightningFadeOut = 0.5 + random * 0.3
```

The visual action is `Delay(thunderDelay) -> FadeTo(0,255) -> FadeTo(lightningFadeOut,0)`. The corresponding `ChooseHeroReadyUIScene::RandomShowwshandianEff(delay,fade)` call is synchronized to those timings.

Finally, the shipped function performs one additional `lrand48()` call whose result is discarded before testing the ground-light node. If `diguang.png` is idle, another random value is consumed and the fade-out duration is:

```text
groundFadeOut = 0.5 + abs(0.01 - random) * 0.3
```

The ground-light action is `Delay(thunderDelay) -> FadeTo(0,255) -> FadeTo(groundFadeOut,0)`.

## Current implementation boundary

`choose_hero_thunder_scheduler.{h,cpp}` now captures the deterministic transforms and draw-order-sensitive planning primitives:

- 31-bit `lrand48` normalization;
- initial 3..8 second thunder delay and 0..0.7 second fade;
- `[A,B,C,C,C,C]` lightning selection;
- optional 7-way thunder sound selection;
- 0.5..0.8 second lightning fade;
- biased 0.5..approximately-0.797 second ground-light fade.

The host smoke test uses fixed raw RNG values to lock these formulas down. This PR intentionally stops before owning the process-global random state, staging `shandian/menlei` textures, or emitting audio. Those are the next integration steps and can now share one verified scheduler contract.
