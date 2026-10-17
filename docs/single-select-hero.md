# SingleSelectHero reconstruction evidence

This note records the clean-room evidence and current implementation boundary for the fresh-install character-selection scene.

## Recovered transition

The original startup path reaches `ManagementLayer::initOpeningDalogue()` when no standalone hero save is available. `OpeningDalogue` is primarily a transition layer:

- `OpeningDalogue::initUI()` starts `sound/SingleSelectHero_bj.mp3` as background music.
- `OpeningDalogue::FuncLayerBegin()` ultimately calls `ManagementLayer::initSelectHero()`.
- `ManagementLayer::initSelectHero()` creates and fades a color layer; the substantial scene content is owned by `SingleSelectHero`.

## Recovered SingleSelectHero base

`SingleSelectHero::init()` loads `SingleLogin_UI/SingleSelectHero/Singleselechero.plist` and calls `SingleSelectHero::initUI()`.

The first confirmed sprite-frame creation order in `SingleSelectHero::initUI()` is:

1. `xrbeijing.png`
2. `xrtengman01.png`
3. `xrtengman02.png`
4. `xrtengman03.png`
5. `xrtengman04.png`

Additional verified frames later in the same function include rune layers, buttons, coffin pieces and hero animation frames.

`xrbeijing.png` is an unrotated full-canvas TexturePacker frame with source size 1137x641 and is the first visual layer created by `initUI()`.

## Current implementation boundary

The recomp currently restores only the confirmed base scene:

- load `xrbeijing.png` from the user-imported `Singleselechero.plist/png` atlas;
- draw it above the SingleLogin scene when the recovered offline startup route is `opening-dialogue`;
- stop the SingleLogin BGM/thunder scene state after that route activates;
- start the imported `sound/SingleSelectHero_bj.mp3` in a loop using the same app lifecycle handling as the other recovered music layers.

No original image or audio bytes are stored in the repository.

## Coffin reveal

`Singleselechero.plist/png` also supplies the coffin interior, door, door trim,
and the standing hero plus its grey and glow layers. The runtime extracts these
frames during the existing atlas load and draws them inside the recovered
274x514 coffin window at `(287, 320)` on the 1136x640 design canvas.

The initial door stays closed during the recovered 84-tick (2.4 second)
carousel. `OpenTheDoor` then supplies a 0.2 second lead-in, 0.3 second pause,
2 second eased movement, and 0.69 seconds of final movement. The compositor
uses that schedule to move the selected door left through the coffin clip,
fade the grey hero layer, and pulse the existing chest-glow frame. Career
changes use the recovered 95-tick closing segment, hold the door shut through
the existing 179/231-tick carousel deadline, then run the same opening motion.
The target points for the original `CCMoveTo` calls are not fully recovered, so
the slide distance remains an estimate sized to clear the coffin window; the
direction, layer order, clip bounds, hero frames, and timing come from the
shipped scene.

The vine stack is intentionally deferred to the next visual increment. `xrtengman01.png` is a rotated TexturePacker frame; the runtime now has a separately regression-tested rotated-frame transform, so the next step can restore all four vines while preserving the recovered insertion order and validating their exact placement independently from this base-scene change.
