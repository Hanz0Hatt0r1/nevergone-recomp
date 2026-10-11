# Offline player joystick dock evidence

This note records the bounded visual boundary for the fixed GameScene joystick in the user-owned Nevergone 1.0.9 ARMv7 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`). Original game images are not stored in Git.

## CCJoyStick child structure

`CCJoyStick::init(int,int,bool,bool,bool,bool)` at ELF `0x00296584` creates three empty `CCSprite` containers and stores them at joystick offsets `+0x104`, `+0x108`, and `+0x10c`. It adds those children in dock, stick, ball order. The recovered initializer does not assign a position to any of the three containers, so the dock remains at the joystick node's local origin. A sprite's default anchor keeps the installed image centered there.

`CCJoyStick::setDockTexture(char const*)` at ELF `0x0029651c` clears the dock container, calls `CCSprite::create(resourceName)`, and adds the resulting sprite as a child of the dock container.

## Controls initializer

`Controls::init()` at ELF `0x00297e2c` creates the fixed joystick, places its node at design coordinate `(204, 173)`, and then calls:

```text
CCJoyStick::setDockTexture("Dock.png")
```

The literal `Dock.png` is present in the immutable ARMv7 library at `.rodata`/file offset `0x769c8d`. The same `Controls::init()` body does not call `setBallTexture` or `setStickTexture`, so this reconstruction does not invent ball/stick artwork.

`AppDelegate::AddAllSearchPath()` separately proves that `controls_ui` is one of the installed Cocos search paths below `<files>/assets`. The clean-room asset request therefore resolves the basename call to the explicit imported-tree path:

```text
controls_ui/Dock.png
```

The base APK does not contain a `Dock.png` ZIP entry. As with the existing GameScene and player asset pipeline, the image is expected from the user's imported/extracted original data rather than committed to this repository.

## Reconstructed rendering boundary

`game_scene_player_joystick_render_queue` appends one direct-file command after the world/player commands:

- resource basename: `Dock.png`;
- imported relative path: `controls_ui/Dock.png`;
- design position: `(204, 173)`;
- rotation: `0`;
- scale: `(1,1)`;
- centered texture geometry.

Appending the command last models the controls layer as a screen-space overlay in the current flat direct renderer. It also keeps the dock inside the existing transactional `GameSceneDirectAssetRequests` revision, so Java staging and native GL drawing cannot disagree about the texture set.

No activated dock variant, movable knob, opacity animation, or additional joystick artwork is assigned here. Strings such as `Dock_activated_01.png` and `Dock_activated_02.png` exist in the binary, but their runtime call path has not yet been proven and is therefore outside this boundary.
