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

The vine stack is intentionally deferred. `xrtengman01.png` is a rotated TexturePacker frame, while the current clean-room atlas extractor intentionally accepts only non-rotated frames. Rotated-frame support should be added and regression-tested before restoring the vine layers, rather than silently rendering them with incorrect orientation.
