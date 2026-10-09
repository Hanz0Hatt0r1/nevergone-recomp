# ChooseHero standalone role-selection contract

This note records clean-room behavior recovered from the shipped ARMv7 `GameSaveData::LoadStandaloneHeroDataList`, `ChooseHero::initSaveDataUI`, `ChooseHeroItem`, and `ChooseHero::updateHeroChooseButton` paths. No original save files, textures, or disassembly dumps are stored in the repository.

## Standalone slot model

`GameSaveData::LoadStandaloneHeroDataList()` iterates slot ids `1` and `2` only. For each slot it formats the writable-path filename as:

```text
DMG_%02d.sData
```

so the shipped standalone filenames are exactly:

- `DMG_01.sData`
- `DMG_02.sData`

The loader writes the current slot id into `SaveDataHero + 0x3c`. `ChooseHeroItem::createChooseHeroItem()` later copies that field into its stored item id/tag, so standalone ChooseHero ids are the slot ids `1` and `2` themselves.

The recompilation therefore no longer treats arbitrary names such as `DMG_00.sData`, `DMG_03.sData`, or `DMG_123.sData` as valid standalone heroes.

## `initSaveDataUI()`

For every loaded `SaveDataHero`, the shipped function:

1. creates a `ChooseHeroItem`;
2. builds the existing-hero item from that save record;
3. places it at X `100`;
4. places the first item at `visibleHeight - 100`;
5. subtracts `ItemHeight + 15` for each subsequent item;
6. adds the item at z-order `3` and stores it in the item array.

The first existing hero is immediately selected, and its `SaveDataHero + 0x3c` slot id becomes the current hero id.

If exactly one standalone hero was loaded, `initSaveDataUI()` appends one `createCreateHeroItem()` using the same vertical spacing. If two heroes were loaded, no create item is added because the recovered standalone capacity is full. The no-save path does not construct this pane in the normal offline route; it goes through the separate opening-dialogue/create-role path.

The clean-room state keeps the item height as a runtime input because the original obtains it from the `CCMenuItemToggle` content size instead of a hard-coded scalar.

## Existing hero item contract

`ChooseHeroItem::createChooseHeroItem()` marks the item as an existing hero and stores its slot id as the selectable tag. Confirmed resources include:

- `hero_board_b.png`
- `hero_board_a.png`
- `Hero_%02d_a.png`
- `Hero_%02d_b.png`
- `sel_hero_name_bg.png`

The two board images are used as toggle states. The `%02d` hero artwork is formatted with the standalone slot id. The item also creates the recovered name/level text layers from `SaveDataHero`; those visual details are intentionally left for the compositor step rather than guessed in the selection model.

## Create item contract

`ChooseHeroItem::createCreateHeroItem()` marks the item as non-existing and stores tag/id `0`. It reuses the same `hero_board_b.png` / `hero_board_a.png` toggle base and adds:

- `create_add_a.png`
- `create_add_b.png`
- localized key `CreateNewHero`

The plus artwork is placed at `(-30, 0)` relative to the board item. The clean-room state uses tag `0` only for this create tile.

## Selection callback

`ChooseHeroItem::menuChooseCallback()` forwards the clicked menu object to `ChooseHero::updateHeroChooseButton()`.

`updateHeroChooseButton()` compares the sender's tag against every item's stored id. The matching item receives `setSelected()` and every other item receives `clearSelect()`.

When the selected item is an existing hero, its stored slot id becomes the current hero id and the preview/player path is updated with that id. When the create tile is selected, the existing visual selections are cleared but the previous current/preview hero id remains intact while the create-role controls are shown. That distinction is preserved by `choose_hero_role_selection_state`.

## Current implementation boundary

`standalone_hero_save_probe` now returns the exact present shipped slots in loader order. `choose_hero_role_selection_state` consumes those slots when the offline TapToStart compatibility path resolves and exposes:

- existing hero items with tags `1` / `2`;
- optional create item with tag `0` only when one save exists;
- first-existing-hero default selection;
- recovered sender-tag selection semantics;
- retained preview id while the create tile is selected;
- the exact X/Y item-placement formula parameterized by runtime item height;
- scene-generation reset and runtime diagnostics.

The next visual step is to stage the confirmed board/hero/create resources from the user-imported original assets, draw these items above the reconstructed ChooseHero background, and route touch hit tests into this already verified state. Scene entry remains a separate later boundary.
