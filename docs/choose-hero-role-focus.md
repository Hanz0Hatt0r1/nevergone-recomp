# ChooseHero selected-item focus reconstruction

This note records clean-room behavior recovered from the shipped ARMv7 `ChooseHeroItem::focesItem()`, `setSelected()`, and `clearSelect()` paths. No original image bytes or disassembly dumps are stored in the repository.

## Resource and lifetime

`focesItem()` creates its focus decoration only when child tag `5` is absent. It creates two `LevelUI/gate_chapter_ui/act_hilight.png` sprites with opacity 0. `setSelected()` calls `focesItem()`, while `clearSelect()` removes child tag `5`, so the effect belongs only to the selected item and restarts when selection changes.

The runtime loads the optional user-owned resource from:

`assets/gamescene_ui/LevelUI/gate_chapter_ui/act_hilight.png`

The focus resource has an independent backing store. Its absence never makes the eight baseline APK-backed role board/icon assets unavailable.

## Recovered action contract

Let `w` be the highlight content width and `W/H` the role board content width/height.

- left X = `100 + w/2`
- right X = `W - w/2`
- intermediate X = `100 + 0.5 * (right - 100)`
- top Y = `H - 11.5`
- bottom Y = `9.5`
- peak opacity = `153`

The top sprite starts at `left` and the bottom sprite starts at `right`. Each runs the same repeating Cocos sequence with mirrored X movement:

1. `CCDelayTime(1.0)`
2. `CCSpawn(CCFadeTo(1.25, 153), CCMoveTo(1.25, intermediate))`
3. `CCSpawn(CCFadeTo(1.25, 0), CCMoveTo(1.25, opposite edge))`
4. `CCMoveTo(0.01, start edge)`
5. `CCDelayTime(2.0)`
6. `CCRepeatForever`

The exact cycle length is 5.51 seconds. The runtime timeline uses the reconstructed 35 Hz game clock and restarts when either the ChooseHero scene generation or the role-selection counter changes.

The original function also creates an additional `hero_board_b.png` child beneath these streaks. The current compositor does not duplicate that board because the selected role item compositor already draws the same verified selected board at the same item position; reusing that existing board is visually equivalent and avoids covering the already reconstructed icon layer with a redundant texture pass.

## Related selected/unselected behavior

Focused ARMv7 inspection also confirms additional `setSelected()` / `clearSelect()` presentation details that remain deliberately separate from this increment:

- selected child tag `1` is hidden and set to scale `1.1`;
- selected child tags `2..4` use `CCScaleTo(0.2, 1.1)` and color `(196,196,196)`;
- cleared child tag `1` becomes visible and uses `CCScaleTo(0.2, 1.0)`;
- cleared child tags `2..4` use `CCScaleTo(0.2, 1.0)` and color `(96,96,96)`;
- clearing selection removes focus child tag `5`.

Name/level labels, `sel_hero_name_bg.png`, and the full `OnCreateback()` scene-entry/create-character branches are still unresolved runtime boundaries and are not approximated here.
