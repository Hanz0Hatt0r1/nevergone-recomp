# SingleSelectHero Confirm control

This note records clean-room evidence for the shipped `SingleSelectHero::menuConfirm` control and its native input/render reconstruction. No original binary or proprietary image bytes are stored in the repository; all visual frames are extracted at runtime from the user-imported original atlas.

## Callback identity

The shipped ARMv7 `SingleSelectHero::initUI()` builds a `CCMenuItemSprite` whose member callback resolves to `SingleSelectHero::menuConfirm` (ELF Thumb address `0x0042745d`; the corresponding checked-in Ghidra view is around `0x0043745c`).

The reconstructed `single_select_hero_state::confirm_online()` preserves the recovered online behavior:

- read the selected career unchanged;
- reject confirmation only when it matches the existing online career;
- otherwise pass the same career integer to `CharacterNameLayer::CretaUI(career)` / `character_name_state::begin(career)`;
- do not invent an `OpenTheDoor` / rune-input gate in `menuConfirm`, because that check is not present in the recovered handler.

## Recovered visual hierarchy

Focused `SingleSelectHero::initUI()` operand recovery resolves the Confirm hierarchy to:

1. `btn_a.png` parent sprite;
2. `btn_b.png` child centered inside that parent;
3. a `CCMenuItemSprite` centered in the same parent using:
   - normal `btn_d.png`;
   - pressed `btn_e.png`;
   - callback `SingleSelectHero::menuConfirm`.

The common world center is `(836,70)` on the 1136x640 Cocos design surface. Equal child z-order preserves the recovered insertion order, so the native compositor draws `btn_a -> btn_b -> btn_d/e`.

The imported `Singleselechero.plist` records the relevant source geometry:

- `btn_a.png`: source `177 x 80`;
- `btn_b.png`: source `156 x 61`;
- `btn_d.png`: trimmed `166 x 75`, untrimmed source/content `170 x 75`, horizontal trim offset `2`;
- `btn_e.png`: source/content `170 x 75`.

`single_select_hero_confirm_layout::quad_for_surface()` applies those TexturePacker trim offsets after the shared centered aspect-fit transform, so visible geometry and touch geometry remain distinct exactly where the atlas trims `btn_d`.

## Exact native hit rectangle

`CCMenuItemSprite` hit testing uses the untrimmed normal content size. Therefore the recovered design-space rectangle is:

```text
center = (836, 70)
size   = 170 x 75
x      = 751 .. 921
```

Cocos uses bottom-origin Y while Android surface input is top-origin. On a 1136x640 surface this becomes:

```text
y = 532.5 .. 607.5
```

The same layout helper scales this rectangle to letterboxed and higher-resolution surfaces.

## Input semantics

`single_select_hero_confirm_input` gets first refusal before the five career-rune controls:

- DOWN/POINTER_DOWN captures only when it starts inside the recovered rectangle;
- MOVE switches pressed presentation off/on when the owning pointer leaves/re-enters;
- secondary pointers are consumed while Confirm owns a gesture and cannot leak into rune input;
- UP/POINTER_UP confirms only when the owning gesture ends inside;
- CANCEL clears the gesture;
- touches outside the Confirm rectangle remain available to the rune router.

A successful UP delegates directly to `single_select_hero_state::confirm_online()`. Existing-career equality blocking and `CharacterNameLayer` entry remain owned by that semantic state.

When `character_name_state` is active, rune/Confirm surface routing and Confirm rendering are disabled. This matches the recovered CharacterName modal blocker and prevents changes behind the name-entry layer.

## Runtime asset staging and GLES rendering

`SingleSelectHeroBaseComposer` extracts `btn_a.png`, `btn_b.png`, `btn_d.png`, and `btn_e.png` from the user-imported `Singleselechero.plist/png` atlas on the GL thread and uploads only decoded runtime pixels to the native compositor. No proprietary bytes are committed.

`single_select_hero_confirm_compositor` keeps generation-independent CPU frame data and lazily creates GLES textures. Each frame uses its exact untrimmed source rectangle plus TexturePacker trim offset. The menu item switches from `btn_d` to `btn_e` only while `single_select_hero_confirm_input::pressed()` is true; the chosen career is not persistently represented by the pressed image.

`GameSurfaceView` already calls `nativeDrawSplashLayers()` immediately after the base SingleSelectHero draw. `post_scene_overlay_jni` registers a small wrapper for that existing JNI slot at library load: it preserves the normal splash draw, then draws the Confirm overlay. This avoids a new Java draw ABI while keeping the Confirm renderer isolated from the base SingleSelectHero compositor.

If registration is unavailable, the existing statically exported splash JNI method remains the fallback; no other native method is replaced.

## Compatibility boundary

Native career runes, rune touch, Confirm touch, and Confirm presentation now own the career-selection portion of the fresh-role path. `FreshRoleCompatOverlay` is hidden in career mode and remains only for the not-yet-native CharacterName text/IME surface.

The next fresh-role presentation block is therefore CharacterName itself: reconstruct the recovered panel/edit-box/buttons using imported assets, route random/confirm/cancel through the existing action executor, and bridge native text focus to Android IME. Once that lands, the compatibility overlay can be removed completely.
