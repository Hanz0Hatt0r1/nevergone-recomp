# SingleLogin light-effect reconstruction

This note records clean-room behavior recovered from the user's original ARMv7 `libcocos2dcpp.so`. No original game assets or decompiled source are stored in the repository.

## Original PRNG behavior

`AppParameters::init()` calls `time(NULL)` and then `srand48(time_value)`. `SingleLoginLayer::InitUI()` and `SingleLoginLayer::Func02()` use `lrand48()` for decorative timing.

The recomp runtime implements the same 48-bit POSIX LCG locally instead of mutating libc-global PRNG state:

- state initialization: `(seed << 16) | 0x330e`
- recurrence: `X = (0x5deece66d * X + 0xb) mod 2^48`
- `lrand48` value: `X >> 17`

## Recovered light nodes

The TexturePacker atlas contains four light frames:

- `zjmdengguang01.png`
- `zjmdengguang02.png`
- `zjmdengguang03.png`
- `zjmdengguang04.png`

All four use the shared 1136x640 source coordinate system. The original `InitUI()` z-order literal array assigns each of these nodes z=5, above the currently reconstructed z=0/z=2/z=3 background stack.

Their atlas trim metadata is preserved at runtime rather than expanding each effect into another full 1136x640 bitmap.

## Initial action chain

For each of the four lights, `InitUI()`:

1. creates the sprite and sets opacity to 0;
2. consumes three `lrand48()` values and uses the third for a random hold;
3. runs `Delay(2.0)`;
4. runs `FadeTo(2.0, 255)`;
5. holds for `5.0 + random * 10.0` seconds;
6. runs `FadeTo(2.0, 0)`;
7. invokes `SingleLoginLayer::Func02`.

This yields an initial random fully-lit hold in the range [5, 15) seconds.

## Recurring Func02 chain

`Func02` stops prior actions, consumes one `lrand48()` value, and runs:

1. `FadeTo(2.0, 255)`;
2. `Delay(5.0 + random * 5.0)`;
3. `FadeTo(2.0, 0)`;
4. callback to `Func02` again.

Thus recurring fully-lit holds are in the range [5, 10) seconds. The project-owned timeline advances callback events chronologically across all four lights so the shared PRNG is consumed in callback-time order rather than renderer iteration order.

## Runtime mapping

`single_login_light_timeline.*` models the recovered actions from the SingleLogin scene boundary. `single_login_compositor.cpp` draws the four user-imported atlas rects at their recovered source positions and applies the sampled opacity through a GLES2 alpha uniform. The effects remain lifecycle-safe because timing is derived from the existing fixed 35 Hz game clock.
