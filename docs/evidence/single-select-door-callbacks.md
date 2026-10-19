# SingleSelectHero callback actions: direct ARM32 evidence

The original Nevergone ARM32 `libcocos2dcpp.so` retains dynamic symbols
for the selection-scene callbacks. They were analyzed directly from the
**original uploaded APK** with Thumb disassembly. The Ghidra-exported
`SingleSelectHero::initUI` and `OpenTheDoor` flow provide independent
callsite and sprite-field context.

Original ELF SHA-256:
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
Its symbol virtual offsets are 0x10000 below the addresses in the
user's imported Ghidra project. Keep each address in its stated space.

## Two six-byte callback trampolines

- `FunOpenTheDoor(CCNode*, void*)`, ELF `0x00428410` /
  Ghidra `0x00438410`: passes boolean **true** and tail-branches
  directly to `OpenTheDoor(bool,int)`.
- `FuncCloseTheDoor(CCNode*, void*)`, ELF `0x00428c14` /
  Ghidra `0x00438c14`: moves its callback data from `r2` into
  argument `r1` and tail-branches to `Carousel(int)`.

These explicit branch instructions corroborate the existing
`single-select-hero-transition-timeline.md` ordering. The callback
does not wait for the newly started opening animation to finish before
`OpenTheDoor(true)` changes the selector interaction gate.

## The initially transparent character child is not a rune

From the original `SingleSelectHero::initUI`:

- Ghidra `0x0043963c`: loads the defined string
  `P01DL_Ani_0001.png` from `0x0078cec2`. The resulting sprite
  is stored at `this+0x1e0`.
- `0x00439676`: `P01DL_Ani_0001_grey.png` (`0x0078ced5`)
  is stored at `this+0x1e4`.
- `0x004396d4`: `P01DL_Ani_0001b.png` (`0x0078ceed`)
  is stored at `this+0x208`, added as a child of the main
  `this+0x1e0` sprite, and set to opacity **0** at
  `0x00439732..0x0043973e`.

The three sprite identities above are recovered from the ELF's actual
defined string bytes. They are not assigned by their similar names
alone.

## FuncBegin(callbackData)

`FuncBegin(CCNode*, void*)` is ELF `0x0042705c`, corresponding to
Ghidra `0x0043705c`. Its callback data in `r2` selects an explicit
mode.

At mode 1 (`0x004270e0..0x00427260` in ELF):

- Several nodes in `this+0x1b4` receive original `CCPoint`
  placements and gameplay/visual actions; a 2-second `CCFadeIn`
  is scheduled for `this+0x1a0`.
- On the branch where `this+0x188 != this+0x1d8`, the child sprite
  stored at `this+0x208` receives a
  `CCSequence(CCDelayTime(6.0), CCFadeIn(0.45),
  CCFadeOut(0.45), CCFadeIn(0.45), CCFadeOut(0.45),
  CCFadeIn(0.45), CCFadeOut(0.45), callback)`.
  The 6.0-second value is the ELF literal at `0x00427300`;
  the 0.45-second value is at `0x00427304`.
- This is the delayed source of the authentic `_0001b` overlay
  animation. Rendering it at full opacity in the initial selector
  frame would contradict the original program.

Modes 2 and 3 walk selected subsets of the `this+0x1b4` node array
and update their positions/actions. Mode 4 manipulates **different**
fields (`this+0x198` and `this+0x19c`) and schedules repeated
opacity ramps between 255 and 127. Mode 5 plays an audio effect.

The source's other timed fades and callbacks are not yet completely
mapped to visual textures/parent clipping. Therefore only the
initially transparent `_0001b` frame is preloaded today; full
renderer integration remains gated on original action ownership.

## Reconstruction impact

- Action-plan geometry and easing are in
  `single_select_hero_coffin_motion.h`.
- The current selector renderer stages `P01DL_Ani_0001b.png`
  and `P02DL_Ani_0001b.png`, but does **not** invent an
  unconditional overlay.
- No original library bytes, screens, assets or private runtime dumps
  are committed.
