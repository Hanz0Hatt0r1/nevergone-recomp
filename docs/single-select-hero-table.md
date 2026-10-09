# SingleSelectHero HeroTable presentation

This note records the clean-room reconstruction of the shipped `SingleSelectHero` career information table. Original atlas bytes and disassembly are not stored in the repository; runtime frames come only from the user-imported Never Gone assets.

## Shipped frames

The baseline `Singleselechero.plist/png` atlas contains four HeroTable frames per picture-language prefix:

```text
%s_HeroTable_01_a.png
%s_HeroTable_01_b.png
%s_HeroTable_02_a.png
%s_HeroTable_02_b.png
```

`SingleSelectHero::initUI()` initially creates `%s_HeroTable_01_a.png`. `SingleSelectHero::Carousel(career)` then formats `%s_HeroTable_%02d_a.png` using the selected career. When the existing career equals the selected career it uses `_b` instead. The reconstructed `single_select_hero_table_layout::frame_index()` preserves that exact choice:

- career 1 normal -> index 0;
- career 1 existing/blocked -> index 1;
- career 2 normal -> index 2;
- career 2 existing/blocked -> index 3.

## Picture-language prefix

`ManagementLayer::GetMultilingualPicturesName()` reads the shipped SystemLanguage value and returns:

```text
enum 2 -> CN
enum 5 -> KR
other  -> EN
```

The Android SystemLanguage bridge derives those values from the system locale, so the runtime picture loader maps:

- `zh` -> `CN`;
- `ko` -> `KR`;
- every other language -> `EN`.

Unlike CSV localization, the picture path does not distinguish simplified/traditional Chinese and does have a dedicated Korean atlas prefix. `SingleSelectHeroPictureLanguage` keeps this rule separate from the text-localization column mapping.

## Recovered position

The shipped `initUI()` positions the HeroTable sprite at exact Cocos design coordinates:

```text
x = 830
y = 320
```

The common recovered design surface is `1136 x 640`. The atlas frame is a trimmed TexturePacker sprite, so the visible texture cannot simply be centered by its trimmed width/height. The compositor places the untrimmed source rectangle at `(830,320)` and then applies the imported frame's `left/top` trim placement.

For the analyzed `EN_HeroTable_01_a.png` frame the imported geometry is:

```text
source: 264 x 443
trim:   264 x 425
left:   0
top:    18
```

which yields the visible design-space rectangle:

```text
left   = 698
top    = 116.5
right  = 962
bottom = 541.5
```

`single_select_hero_table_layout` maps that rectangle through the same centered aspect-fit `1136 x 640` viewport used elsewhere in the reconstructed UI. The host smoke verifies the exact design surface, a 2x surface, and a letterboxed 16:9 surface.

## Runtime asset lifecycle

`SingleSelectHeroBaseComposer.composeBackground()` already runs on the GL thread while loading the user-imported `Singleselechero.plist/png`. It now also extracts the four HeroTable frames for the current picture-language prefix and uploads only their decoded pixels plus TexturePacker geometry to the native compositor.

The native compositor keeps CPU backing for context recreation and creates GLES textures lazily. When the fresh offline route is inactive it drops the scene-owned texture names but keeps reloadable CPU data, matching the existing SingleSelectHero background lifecycle.

## State integration

The fresh `OpeningDialogue` route is reached only when the standalone-hero probe found no existing slots. `nativeIsSingleSelectHeroActive()` therefore begins `single_select_hero_state` with existing career `0` on first entry. The recovered state selects career `1` and enters its initial locked Carousel transition.

The compositor reads `single_select_hero_state::snapshot()` every frame, selects the matching HeroTable `_a/_b` variant and draws it above `xrbeijing.png`. No visual state is duplicated inside the renderer.

## Current boundary

This increment restores the visible career information table only. It does not yet render the `P%02dDL_Ani_0001*` character/grey/overlay sprites, finish the door/carousel animation, or add career/confirm touch targets. Those remain separate evidence-driven increments so the interaction geometry is not guessed from appearance.
