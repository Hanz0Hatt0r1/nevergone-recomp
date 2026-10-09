# SingleSelectHero Confirm control

This note records clean-room evidence for the shipped `SingleSelectHero::menuConfirm` control and the native hit-routing increment built from it. No original binary or proprietary image bytes are stored in the repository.

## Callback identity

The shipped ARMv7 `SingleSelectHero::initUI()` builds a `CCMenuItemSprite` whose member callback resolves to `SingleSelectHero::menuConfirm` (ELF Thumb address `0x0042745d`; the corresponding checked-in Ghidra view is around `0x0043745c`).

The existing reconstructed `single_select_hero_state::confirm_online()` already preserves the recovered online callback behavior:

- read the selected career unchanged;
- reject confirmation only when it matches the existing online career;
- otherwise pass the same career integer to `CharacterNameLayer::CretaUI(career)` / `character_name_state::begin(career)`;
- do not invent an `OpenTheDoor` / rune-input gate in `menuConfirm`, because that check is not present in the recovered handler.

## Resources and parent geometry

Focused `initUI()` operand recovery resolves the control resources to:

- normal: `btn_d.png`;
- pressed: `btn_e.png`.

The control is centered inside the `btn_a.png` parent panel. That parent is positioned at design coordinates:

```text
(836, 70)
```

on the 1136x640 Cocos design surface. The menu item's local half-content positioning therefore gives the Confirm item the same world center `(836, 70)`.

The imported `Singleselechero.plist` records:

- `btn_d.png`: trimmed sprite `166 x 75`, untrimmed source/content size `170 x 75`, two horizontal trim pixels;
- `btn_e.png`: source/content size `170 x 75`.

Cocos `CCMenuItemSprite` hit testing uses the untrimmed normal content size, not the trimmed visible quad.

## Exact native hit rectangle

The recovered design-space content rectangle is therefore:

```text
center = (836, 70)
size   = 170 x 75
x      = 751 .. 921
```

Cocos uses bottom-origin Y while Android surface touch coordinates are top-origin. On a 1136x640 surface this maps to:

```text
y = 532.5 .. 607.5
```

`single_select_hero_confirm_layout` applies the same centered aspect-fit transform used by the rest of the reconstructed SingleSelectHero surface, so the rectangle scales correctly on letterboxed or higher-resolution surfaces.

## Input semantics

`single_select_hero_confirm_input` gives the Confirm control first refusal before the five rune controls:

- DOWN/POINTER_DOWN only captures when it starts inside the recovered rectangle;
- MOVE toggles the pressed state as the pointer leaves or re-enters the same control;
- UP/POINTER_UP confirms only when the captured gesture ends inside;
- CANCEL clears the gesture;
- touches outside the Confirm rectangle remain available to the rune router.

A successful UP delegates directly to `single_select_hero_state::confirm_online()`; equality blocking and `CharacterNameLayer` entry remain owned by that existing semantic state.

When `character_name_state` is active, SingleSelectHero rune/confirm surface routing is disabled. This corresponds to the recovered CharacterName modal blocker and prevents changing career behind the name-entry layer.

## Current rendering boundary

This increment connects the exact native hit region and action only. It does not yet render `btn_d.png` / `btn_e.png` in GLES.

The next presentation increment should:

1. extract the two frames from the user-imported `Singleselechero.plist/png` atlas;
2. draw the trimmed normal/pressed quads at the recovered `(836,70)` center;
3. bind the pressed frame to `single_select_hero_confirm_input::pressed()`;
4. preserve the existing scene-owned GPU lifecycle.

Until that renderer lands, the temporary Android fresh-role compatibility overlay remains available as the visible Confirm presentation, while the exact native surface interaction contract is now in place.
