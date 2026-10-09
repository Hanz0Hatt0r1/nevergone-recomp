# ChooseHero role-item compositor

This note documents the first visual/touch layer built on the recovered standalone `ChooseHero` selection state. It uses only user-imported original PNG files at runtime; no original image bytes are stored in the repository.

## Imported resources

The baseline APK contains the following direct PNGs under `assets/Login/ChooseHero/`, and `OriginalApkImporter` already decodes PNG assets into app-private `files/assets` storage:

- `hero_board_a.png`
- `hero_board_b.png`
- `Hero_01_a.png`
- `Hero_01_b.png`
- `Hero_02_a.png`
- `Hero_02_b.png`
- `create_add_a.png`
- `create_add_b.png`

`ChooseHeroRoleAssetLoader` decodes those files with Android `BitmapFactory` and uploads ARGB pixels into a synchronized native CPU backing store. Width and height come from the imported files at runtime rather than from hard-coded metadata. A diagnostics refresh reloads the store, so an APK import performed after launch becomes visible without restarting the process.

## Recovered toggle semantics

`ChooseHeroItem::createChooseHeroItem()` and `createCreateHeroItem()` construct a `CCMenuItemToggle` from two reversed `CCMenuItemImage` states:

- the selected state uses `hero_board_b.png` as its normal image;
- the cleared/unselected state uses `hero_board_a.png` as its normal image.

`setSelected()` chooses toggle index `0`, while `clearSelect()` chooses index `1`.

The item child sprites follow the same A/B distinction. The existing-hero variants are `Hero_%02d_a/b.png`, and the create tile uses `create_add_a/b.png`. Recovered item code places those sprites at local position `(-30, 0)`. The compositor therefore draws:

- unselected hero: `hero_board_a` + `Hero_0N_a`;
- selected hero: `hero_board_b` + `Hero_0N_b`;
- unselected create tile: `hero_board_a` + `create_add_a`;
- selected create tile: `hero_board_b` + `create_add_b`.

The role ids/tags themselves come from `choose_hero_role_selection_state` and remain the shipped standalone slot ids `1` and `2`; the create tile is tag `0`.

## Placement and hit testing

`ChooseHero::initSaveDataUI()` positions each `ChooseHeroItem` at X `100`. The first Y is `visibleHeight - 100`; each following item subtracts `ItemHeight + 15`. `ItemHeight` is the `CCMenuItemToggle` content height, so the runtime compositor derives it from the imported board image instead of freezing the observed baseline size in code.

The original menu item is centered at its node position. The reconstructed hit rectangle therefore uses the imported board width/height centered on the recovered item position.

Android surface coordinates are converted into the 1136x640 design canvas with aspect-fit letterboxing before hit testing. A DOWN/UP pair must begin and end on the same item before its recovered sender tag is passed to `choose_hero_role_selection_state::select_tag`. While the offline `choose-role` route owns input, those touches are consumed before the pre-existing server/TapToStart paths.

## Rendering order

The existing ChooseHero GL callback already reconstructs:

1. PartThree storm background (z=10);
2. thunder/lightning/ground-light effects (z=20);
3. `BalckCloud` foreground clouds (z=30).

The item pane is UI rather than another background-effect node, so it is drawn after those scene layers. The compositor keeps its GLES textures generation-aware and recreates them if imported asset generation changes or an EGL context invalidates the old texture names.

## Deliberate boundary

This layer does **not** approximate the remaining `ChooseHeroItem::focesItem()` behavior. In particular, it does not yet draw or animate:

- `sel_hero_name_bg.png`;
- save-derived hero name and level labels;
- the exact focus scale/color action sequence;
- Play/Create buttons and their scene-entry callbacks.

Those are separate evidence-driven increments. The current layer establishes the original board/icon appearance, recovered item positions, runtime-sized hit boxes, and exact selection tags without inventing the unresolved presentation logic.
