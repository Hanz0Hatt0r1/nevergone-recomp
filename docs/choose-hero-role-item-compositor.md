# ChooseHero role-item compositor

This note documents the reconstructed standalone `ChooseHero` role pane. It uses only user-imported original PNG files at runtime; no original image bytes are stored in the repository.

## Imported resources

The baseline APK contains direct PNGs under `assets/Login/ChooseHero/`, including:

- `hero_board_a.png`
- `hero_board_b.png`
- `Hero_01_a.png`
- `Hero_01_b.png`
- `Hero_02_a.png`
- `Hero_02_b.png`
- `create_add_a.png`
- `create_add_b.png`
- `sel_hero_name_bg.png`

`ChooseHeroRoleAssetLoader` decodes the board/hero/create files with Android `BitmapFactory` and uploads ARGB pixels into a synchronized native CPU backing store. Width and height come from imported files at runtime rather than hard-coded metadata. `ChooseHeroProfileLabelLoader` separately loads `sel_hero_name_bg.png` and rasterizes the recovered localized profile labels.

## Recovered toggle semantics

`ChooseHeroItem::createChooseHeroItem()` and `createCreateHeroItem()` construct a `CCMenuItemToggle` from two reversed `CCMenuItemImage` states:

- selected uses `hero_board_b.png`;
- cleared/unselected uses `hero_board_a.png`.

`setSelected()` chooses toggle index `0`, while `clearSelect()` chooses index `1`.

The item child sprites follow the same A/B distinction. Existing heroes use `Hero_%02d_a/b.png`; the create tile uses `create_add_a/b.png`. Recovered item code places those sprites at local `(-30, 0)`. The role ids/tags remain standalone slot ids `1` and `2`; create is tag `0`.

## Placement and hit testing

`ChooseHero::initSaveDataUI()` positions each item at X `100`. The first Y is `visibleHeight - 100`; each following item subtracts `ItemHeight + 15`. `ItemHeight` comes from the runtime board image, matching the original content-size dependency.

The original menu item is centered at its node position. Android surface coordinates are converted into the 1136x640 design canvas with aspect-fit letterboxing before hit testing. A DOWN/UP pair must begin and end on the same item before its recovered sender tag is passed to `choose_hero_role_selection_state::select_tag`.

## Rendering order

The ChooseHero GL callback reconstructs:

1. PartThree storm background (z=10);
2. thunder/lightning/ground-light effects (z=20);
3. `BalckCloud` foreground clouds (z=30);
4. role board/icon tiles;
5. save-derived name background/name/level/GameUSETime profile content;
6. selected-item `focesItem()` highlight streaks.

All GLES stores are generation-aware and recreate texture names after imported asset changes or EGL context loss.

## Profile and focus layers

The selected-item `focesItem()` action is reconstructed separately from the static tile: two mirrored `act_hilight.png` streaks run the shipped 5.51-second repeat sequence and restart when selection changes.

Existing-hero profile data is also independent of the selection model. Valid `DMG_01/02.sData` metadata supplies level and played time; `Login/ALL_Loin.csv` supplies localized `Hero%dName`, `GdUI08` and `GameUSETime` strings. Labels use the recovered `Arial` size 20 Android/Cocos rasterization behavior and node color `(96,96,96)`.

The exact profile hierarchy/anchors are documented in `docs/choose-hero-save-metadata.md`.

## Remaining boundary

The role pane now has recovered board/icon selection, focus effects and save-derived profile presentation. Play/Create controls and their `ChooseHero::OnCreateback()` / selected-role scene-entry behavior remain intentionally unresolved rather than approximated.
