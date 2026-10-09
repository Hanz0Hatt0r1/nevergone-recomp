# CharacterNameLayer recovered layout

This note records clean-room geometry recovered directly from the shipped ARMv7 `CharacterNameLayer::CretaUI(int)` implementation. It complements `docs/character-name-layer.md`: state/action semantics live there, while this document fixes only presentation geometry and hit-test relationships. No original image bytes or disassembly dumps are stored in the repository.

## Recovered constants

The shipped function contains these literal placement constants:

- background vertical offset from the visible top: `188`;
- gap between the bottom of `Redbottom.png` and `RANDOMName.png`: `10`;
- Confirm/Cancel horizontal side inset: `135`;
- Confirm/Cancel center Y: `52`;
- gap from the right edge of the name plate to the random button center: `40`;
- edit-box height: `30`.

The implementation obtains all sprite/button widths and heights from the created Cocos nodes. The recompilation therefore keeps image dimensions as runtime inputs instead of baking measurements from one asset build.

## Background and title

`Login/ChooseHero/Redbottom.png` is placed at:

```text
x = visibleWidth / 2
y = visibleHeight - 188
```

The localized `Prompt9` title is placed at the same recovered center.

## Name plate and edit box

`ServerList/RANDOMName.png` is horizontally centered and placed directly below the red background:

```text
x = visibleWidth / 2
y = backgroundY - backgroundHeight / 2 - 10 - namePlateHeight / 2
```

The transparent `TouMing.png` scale-9 edit box is created with:

```text
width  = namePlateWidth
height = 30
center = namePlateCenter
```

This matches the shipped construction, where the edit box reads the `RANDOMName` content width and reuses that node's position.

## Random-name control

The `ServerList/RANDOM.png` menu item is positioned to the right of the name plate:

```text
x = namePlateX + namePlateWidth / 2 + 40
y = namePlateY
```

Its recovered callback tag is `3`.

## Confirm and Cancel

Both controls use `createMRFixedButton(type=1, ...)`. Their exact centers are symmetric:

```text
Confirm: (visibleWidth - 135, 52), tag 1
Cancel:  (135,                52), tag 2
```

Hit boxes are derived from the imported fixed-button content dimensions.

## Modal blocker

The function also creates a `Button_C_a.png` / `Button_C_b.png` menu item, centers it at `(visibleWidth/2, visibleHeight/2)`, scales it independently in X and Y to cover the visible surface, assigns tag `5`, and makes it transparent. Tag `5` is not a CharacterName action; this item is retained semantically as the modal touch blocker behind the actionable controls.

## Clean-room implementation

`character_name_layout` now exposes these formulas as a renderer-independent layout:

- background/title center;
- name plate and 30-pixel edit box;
- random-name button;
- Confirm and Cancel controls;
- full-surface modal blocker;
- content-size-based hit testing.

`tools/character_name_layout_smoke.cpp` checks the recovered constants, relative placement, boundary-inclusive hit tests, and rejection of invalid dimensions. The next compositor can consume this module without re-encoding geometry or mixing it with `CreateCharacter` execution.
