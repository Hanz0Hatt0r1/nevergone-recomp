# Offline player joystick dock evidence

This note records the bounded visual boundary for the fixed GameScene joystick in the user-owned Nevergone 1.0.9 ARMv7 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`). Original game images are not stored in Git.

## CCJoyStick child structure

`CCJoyStick::init(int,int,bool,bool,bool,bool)` at ELF `0x00296584` creates three empty `CCSprite` containers and stores them at joystick offsets `+0x104`, `+0x108`, and `+0x10c`. It adds those children in dock, stick, ball order. The recovered initializer does not assign a position to any of the three containers, so the dock remains at the joystick node's local origin. A sprite's default anchor keeps the installed image centered there.

`CCJoyStick::setDockTexture(char const*)` at ELF `0x0029651c` clears the dock container, calls `CCSprite::create(resourceName)`, and adds the resulting sprite as a child of the dock container.

## Controls initializer and active-state switch

`Controls::init()` at ELF `0x00297e2c` creates the fixed joystick, places its node at design coordinate `(204, 173)`, and calls:

```text
CCJoyStick::setDockTexture("Dock.png")
```

The literal `Dock.png` is present in the immutable ARMv7 library at `.rodata`/file offset `0x769c8d`. The same `Controls::init()` body does not call `setBallTexture` or `setStickTexture`, so this reconstruction does not invent ball/stick artwork.

The active/inactive dock transition is also direct native evidence:

- `Controls::onCCJoyStickActivated(CCNode*)` at ELF `0x00296760` verifies the callback node is the retained joystick at `Controls +0x124`, then calls `setDockTexture("Dock_s.png")`; its PC-relative literal resolves to `.rodata`/file offset `0x769c56`.
- `Controls::onCCJoyStickDeactivated(CCNode*)` at ELF `0x002970d4` performs the same retained-node check, then calls `setDockTexture("Dock.png")` before clearing the live Controls sample/state fields.

Therefore the evidence-backed visual state is:

```text
not captured -> Dock.png
captured     -> Dock_s.png
release      -> Dock.png
```

`Dock_activated_01.png` and `Dock_activated_02.png` also exist as strings in the library, but they are not arguments to these `setDockTexture` call sites and are not assigned to this joystick boundary.

`AppDelegate::AddAllSearchPath()` separately proves that `controls_ui` is one of the installed Cocos search paths below `<files>/assets`. The clean-room asset requests therefore resolve the basename calls to explicit imported-tree paths:

```text
controls_ui/Dock.png
controls_ui/Dock_s.png
```

The base APK does not contain either dock PNG as a ZIP entry. As with the existing GameScene and player asset pipeline, the images are expected from the user's imported/extracted original data rather than committed to this repository.

## Reconstructed rendering boundary

`game_scene_player_joystick_render_queue` appends one direct-file command after the world/player commands. It reads the already recovered joystick capture state and chooses:

- inactive resource: `Dock.png` / `controls_ui/Dock.png`;
- active resource: `Dock_s.png` / `controls_ui/Dock_s.png`;
- design position: `(204, 173)`;
- rotation: `0`;
- scale: `(1,1)`;
- centered texture geometry.

Appending the command last models the controls layer as a screen-space overlay in the current flat direct renderer. The selected path participates in the existing `GameSceneDirectAssetRequests` hash, so touch activation/deactivation changes the transactional revision. Java staging and native GL drawing therefore cannot silently retain the inactive image after the joystick becomes active, or vice versa.

A movable knob/stick sprite, opacity animation, and any use of the two `Dock_activated_*` strings remain outside this boundary until their actual runtime path is proven.
