# SingleSelectHero: coffin base and reel/lid evidence

Source: user-owned original Nevergone 1.0.9 ARM32 APK plus focused Ghidra exports
(`SingleSelectHero::initUI`, `Carousel`, `OpenTheDoor`).
The original asset bytes and disassembly remain outside this repository.

Evidence identities:
- APK SHA-256: `c8aaabfe28893998aaf891716147328245e16a043d95b7abdeac43ef6adc9cab`.
- Local focused export archive SHA-256:
  `504e3c56d5688ea98d63181adffc68cb544303f481c841b4aeb55345b3cf358c`.
- Original `libcocos2dcpp.so` SHA-256:
  `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Proven construction and geometry

The original imported `Singleselechero.plist` records:

| frame | untrimmed size | rotated |
| --- | --- | --- |
| `xrshiguan.png` | 274x514 | yes |
| `xrguangai.png` | 271x504 | no |

`SingleSelectHero::initUI` at `0x00438c20`:

- `0x00439344..0x0043939e`: creates `xrshiguan.png`;
  sets position `(150 + 274 * 0.5, visibleHeight * 0.5)` = `(287,320)`
  at the 1136x640 design resolution; adds the sprite at z=2.
- `0x004393a0..0x004393ac`: retrieves/copies that sprite position.
- `0x004393b0..0x004393d0`: creates the movable reel `CCNode`,
  an array of cover sprites, and a `CCPointArray` control-point list.
- `0x004393da..0x00439454`: loop creates five `xrguangai.png`
  cover sprites. Their centers are `(287 + 271*i, 320)` for `i=0..4`.
  Each cover is a child of the reel node at z=3; each center is also
  appended to the control-point array.
- `0x00439624..0x00439674`: creates the hero-holder `CCNode`
  as a child of the reel at z=2; creates the first body sprite in that holder.
  The first body copies the `(287,320)` position from the fixed base.

`SingleSelectHero::Carousel` at `0x004388ac`:

- `0x004389ce..0x00438a16`: indexes the selected career's control
  point, retrieves point zero, and subtracts X values to obtain the
  target reel X displacement.
- `0x00438a5c..0x00438abc`: schedules `CCMoveTo`,
  `CCEaseInOut`, `CCEaseSineOut`, `CCEaseSineIn` and a callback
  sequence on the reel.
- `0x00438ac0..0x00438ac8`: counter-positions the hero holder
  by negating the reel displacement. This establishes the center
  relationship for the settled selected preview without inventing
  a second hero position.
- The known transition durations are recorded separately in
  `docs/single-select-hero-transition-timeline.md`.

`SingleSelectHero::OpenTheDoor` at `0x00437f00`:

- `0x00437f0a..0x00437f32`: selects the matching cover object,
  retrieves its point, and tests `numberOfRunningActions`.
- `0x00437f5c` branches on the requested open/close mode.
- `0x00437fa0..0x00438082`: one mode schedules a compound cover
  sequence containing `CCMoveTo`, sine eases, an explicit 0.5-second
  delay, and callbacks.
- `0x004381b0..0x00438390`: the other branch contains its own
  movement and opacity operations.
  Further target/value joins are required before rendering it.

## Implementation boundary

- Native `single_select_hero_compositor` draws `xrshiguan.png`
  in the background at its proven `(287,320)` center, behind the
  character body.
- The same APK atlas importer also stages the `xrguangai.png` frame,
  so five lid copies can be constructed without another OBB search.
- Lid offsets, action timing and animated sprite quads are modeled
  in `single_select_hero_coffin_layout.h`,
  `single_select_hero_coffin_motion.h` and
  `single_select_hero_lid_presentation.h`.
- The native compositor now draws the single **active** lid as a foreground
  occluder during the recovered intro and uninterrupted career-change
  sequences. It draws the body above the fixed base but below the active
  lid. The lid pose remains available for 3.39 s after the input gate
  unlocks.
- Early overlapping career changes have ambiguous starting poses due to
  the native `numberOfRunningActions()` guard. The visual timeline
  therefore marks them `initial_positions_verified=false` and draws
  no lid, without changing the input/career selection behavior.
- This is conservative partial scene recovery, not full parity of
  simultaneous covers, original clipping and other callback effects.
- No original assets, binary bytes or raw instructions are committed.
  All original APK/OBB files, user-imported resources and saves are
  untouched.

## Next evidence task

Recover the door action's exact target `CCPoint` values, callback
identity and cover open/closed state at scene entry using the existing
`00437f00.pcode-flow.tsv` / `selection.instructions.jsonl.gz`
plus bounded data references. Only then animate the five lids.
