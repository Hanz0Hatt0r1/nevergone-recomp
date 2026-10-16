# Player clothes and sprite-frame lookup (2026-10-16)

Original ARMv7 ELF SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`. Only derived behavior and resource names are retained here; original assets and binary bytes stay local.

## Corrected native contract

The earlier initial-frame and preload-family notes omitted a branch of `MeActionsSystem::updateData()` and incorrectly inferred clothes from the action plist filename.

- `MEPlayer::createActions()` at `0x31906e` reads the clothes halfword at player `+0x194`. The call at `0x319092` passes it independently of the action filename, through `creinitWithFile()` to `initWithFile()`.
- `creinitWithFile()` moves that stack argument to the instance-call argument at `sp+4` (`0x328896..0x32889c`). `initWithFile()` reads it at `0x32bd2e` and stores it at action system `+0x30c` at `0x32bd44`.
- `updateData()` reads `+0x30c` at `0x329da4`. Zero takes the original frame key directly. Nonzero takes the branch at `0x329dc4`, formatting the clothes id and original frame key with `%02d%s` (read-only string at `0x771530`).
- The prefixed key is looked up at `0x329de6`. A null result branches back at `0x329dee` to the unprefixed lookup at `0x329dbc`.
- The preload family independently uses the same player `+0x194` clothes selector with `p%02d_%02d_%02d_default.plist`, members 1 through 9. ActionFrameData `+0x60` is an additional atlas request, not a reliable clothes selector.

## Reconstruction

The supported fresh-role construction uses the already recovered construction clothes id 1 from `game_scene_player_bootstrap.h`. Existing-role handoffs now retain the parsed `ClothesID`; missing or out-of-range existing-role values remain unknown instead of borrowing the fresh-role default. Later wardrobe/color changes remain outside this patch.

Career and clothes id travel with the player render request and participate in its revision. The Java cache resolver uses those explicit values to preload the family, appends an available action-specific plist, then resolves the prefixed frame before the original key. Cache identity includes career and clothes, so two clothing selections cannot share a prepared cache accidentally.

## Validation

Host tests cover independently selected clothes/action plist families, prefixed-name precedence, unprefixed fallback, missing frames, role handoff retention, fresh-role selection and request revision changes when clothes change. The Android debug build passes for both configured ABIs.

Against the complete extracted user-owned OBB, the production Java resolver loaded 8 available clothes-1 atlases and resolved the initial `P01_I01_001.png` request to `01P01_I01_001.png` in `player/Player01_Res/p01_01_01_default.plist`.

## Physical device

The integrated APK (including tutorial resource/localization fixes) was installed with data preserved on the Redmi Note 8 Pro (Android 13, arm64). After entering the fresh-role tutorial and dismissing its dialogue, the player rendered at the recovered scene point `(430, 271)`. Runtime evidence at `/tmp/nevergone-integrated/runtime-report.txt` passed at `2026-10-16T12:26:44Z`: 9 sprites drawn, 9 requests staged, and no hidden slots. Diagnostics SHA-256: `c5f4800cb45352c10c23e31ef423a0e90c37b845916b13e1c4cfca9e9af4d956`; screenshot SHA-256: `e620efa569ef7879a54bd5c4c8ce2aef5bd8f7c0aa910a87fefa861a6c5a0b2e`.

### P8/P9 contiguous device proof

After the tutorial dialogue was dismissed, a 1.2-second rightward joystick gesture moved the player visibly from the left side of the lit floor to its right side. Before and after screenshots are retained locally at `/tmp/nevergone-before-input.png` and `/tmp/nevergone-after-input.png`, with SHA-256 `1f3d9d657b53a94ff1851ebf91e95e1a74b5e15048b75276ed656511517ab500` and `693a335cc1db15848281756d37f23832e3d6be7b61d85dddc676badfb78c4acc`, respectively. Refreshed runtime diagnostics recorded 2 accepted joystick begins and 73 moves in the same scene. The motion counter was zero after release, so this proves visible gesture-driven movement, not persistent motion after release. The validated post-gesture scene report and screenshot remain at `/tmp/nevergone-integrated/`.

Palette recoloring, equipment layers and full visual fidelity remain outside this body-frame fix.
