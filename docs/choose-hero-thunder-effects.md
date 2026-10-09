# ChooseHero thunder/lightning effect runtime

This note records the next clean-room runtime layer built on the recovered scheduler in `docs/choose-hero-thunder-scheduler.md` and the staged PartThree effect assets.

## Proven placement and z-order

Focused Thumb/ARM inspection of `ChooseHeroBackground::PartThree()` confirms that all six `shandian%02d.png` sprites and all six `menlei%02d.png` sprites are positioned at half the visible width/height, use their normal sprite anchor, start at opacity 0, and are added at z-order 20. `diguang.png` is also centered, opacity 0, z-order 20.

Creation order at the shared z-order is:

1. `shandian01..06`
2. `menlei01..06`
3. `diguang`

The runtime compositor preserves that insertion order between the storm layer at z=10 and `BalckCloud` at z=30.

## Stateful action machine

`choose_hero_thunder_state` turns the already verified raw scheduler formulas into Cocos-style running actions while remaining host-testable.

On scene start it seeds a local POSIX-48 compatible stream using the original `srand48(seed)` state formula. Exact cross-run values are not claimed to reproduce the original process-global stream because other original subsystems also consumed that stream before ChooseHero; the recovered draw order and transforms are preserved.

Initial `RamodThunder()` scheduling consumes, for each of six `menlei` sprites:

- six discarded draws;
- one delay draw;
- one fade draw.

That is exactly 48 draws before the first callback.

At each thunder flash the machine consumes the `FuncThunderBen` sound-selection draw. At each thunder fade completion it applies the canonical `FuncThunderEnd` reschedule plan. The `[A,B,C,C,C,C]` lightning selection is evaluated against current running-action state, so repeated selections of a sprite already scheduled by the same callback do not consume additional fade RNG values. The unconditional discarded ground-light draw is also retained before the ground-light idle check.

The machine exposes alpha snapshots for six thunder sprites, six lightning sprites, and ground light. Alpha stays zero during delay, jumps to full opacity at the recovered zero-duration `FadeTo(255)`, then decreases linearly over the recovered fade-out duration.

## Rendering

`choose_hero_thunder_effect_compositor` consumes staged slots:

```text
6      diguang.png
10..15 shandian01..06.png
16..21 menlei01..06.png
```

Each TexturePacker frame is restored inside its untrimmed source rectangle, centered on the 1136x640 design canvas exactly as the original Cocos sprite was positioned. Aspect-fit mapping matches the existing ChooseHero compositors.

The compositor recreates all 13 GLES textures when the staged asset generation changes or an EGL context recreation invalidates old texture names.

## Audio boundary

`FuncThunderBen` sound selection is already consumed by the state machine so RNG order remains correct. Valid sound slots are drained by the compositor but intentionally not played yet; diagnostics count these suppressed sound callbacks. This prevents an unbounded pending queue while keeping the next audio integration isolated.

The next increment should route those slots to the already user-imported `sound/SingleLogin_UI/...` files without changing scheduler timing or PRNG consumption.

## Verification

`tools/choose_hero_thunder_state_smoke.cpp` pins a fixed POSIX-48 seed and verifies:

- the initial 48-draw setup;
- the first two sound callbacks;
- a silent sound selection followed by a valid slot;
- the first `FuncThunderEnd` reschedule;
- running-action gating for repeated lightning indices;
- the unconditional ground-light draw and scheduled ground-light action.

The normal Android workflow additionally compiles the GLES compositor for both configured ABIs and validates the debug APK and 16 KiB page compatibility.
