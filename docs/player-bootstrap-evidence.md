# Offline MEPlayer bootstrap evidence

This note narrows issue #148 from first-scene position recovery to the minimum player construction/resource contract. It uses derived instruction-level observations from the user-owned Nevergone 1.0.9 ARMv7 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`). The original binary and assets remain outside Git.

## Selected Career is the offline player index

`SingleSelectHero::menuConfirm(CCObject*)` provides the decisive offline initialization path.

The selected Career value is read from the `SingleSelectHero` state and retained in `AppParameters + 0x258`. Around ELF `0x004274d4` that same value is loaded as the second argument to `MEPlayerManager::sharedMEPlayerManager(nullptr, Career)`, called at `0x004274e6`. Immediately afterward the primary `MEPlayer` is obtained, the same retained Career is loaded again, and `MEPlayer::setPlayerIdx(Career)` is called at `0x00427504`.

The recovered SingleSelectHero domain is Career `1..5`. Therefore the standalone/offline player index is the selected Career value in that domain. A later `GameScene::initGameScene()` call with manager argument `0` must not be interpreted as Career 0: `sharedMEPlayerManager` is a singleton and only applies its initialization arguments when creating the manager.

## First MEPlayer construction

`MEPlayerManager::sharedMEPlayerManager(GameScene*, unsigned int)` at ELF `0x003254a8` allocates and initializes the singleton only when it does not already exist.

`MEPlayerManager::initMEPlayerManager(GameScene*, unsigned int)` at ELF `0x003253bc`:

- stores the supplied `GameScene` pointer;
- clears the primary/secondary player slots;
- obtains `CCDirector::getWinSize()`;
- constructs the initial point `(0.5 * win_width, 0.7 * win_height)`;
- creates the name string `SELF`;
- calls `MEPlayer::creinitWithMEName("SELF", GameScene, point, player_idx, false, 1, 0)` around `0x00325440`;
- stores and retains the returned primary player.

This point is an allocation/bootstrap point, not the recovered first-scene entry position. The type-6 `GameScene::loadingTex()` path documented in `player-spawn-metadata-evidence.md` later writes the transformed entry point into the live player-position cursor and calls `MEPlayer::setPlayerPos()`.

`game_scene_player_bootstrap::initial_manager_point()` models only this proven bootstrap point and rejects non-finite/non-positive window dimensions.

## Player-data resource selector flow

`MEPlayer::setPlayerIdx(unsigned int)` at ELF `0x00324e94` stores the player index at `MEPlayer + 0x144`, enters action state `0x1b`, loads the already-retained halfword at `MEPlayer + 0x194`, and tail-calls `MEPlayer::setColorFile(unsigned int)`. It does not perform the player-data lookup itself.

The data-population path is `MEPlayer::loadPlayerData()` at ELF `0x00324eb2`. It obtains the player-data object from the recovered `DataManager + 0x44` path, invokes two virtual getters, stores their returned halfwords at `MEPlayer + 0x194` and `MEPlayer + 0x198`, then calls `setColorFile()` with the `+0x194` value.

`MEPlayer::setColorFile(unsigned int)` starts at ELF `0x003249d8`. It compares its incoming selector against the separate halfword at `MEPlayer + 0x198`; when they differ, the function proceeds through the recovered color/resource-file loading and parsing path using the player index and incoming selector. The current evidence does not justify naming `+0x198` more specifically, nor does this bounded disassembly establish a direct `setColorFile() -> initPlayerRes()` call.

The field at `MEPlayer + 0x194` is independently confirmed as a resource selector by `MEPlayer::createActions(unsigned int)`, which reads that exact halfword as the second formatter argument for the default sprite plist names. The reconstruction therefore keeps this selector as an explicit input until the corresponding standalone player-data value is recovered; it must not invent a zero/one default.

## Action and default sprite plist naming

`MEPlayer::initActionsSystem()` loads `MEPlayer + 0x144`, subtracts one, and calls `MEPlayer::createActions(player_idx - 1)` around ELF `0x00322330`.

`MEPlayer::createActions(unsigned int)` at ELF `0x00318d8c` reconstructs the one-based player number and formats resource names with the retained `MEPlayer + 0x194` selector:

```text
p%02d_actions.plist
p%02d_%02d_%02d_default.plist
```

For the default sprite-frame plists the third number iterates from `1` through `9`. The first number is the selected offline Career/player index. The second number is the retained `+0x194` resource selector. `%02d` supplies a minimum field width, so the project-owned formatter preserves larger recovered ids rather than inventing a `0..99` range restriction.

`game_scene_player_bootstrap::resource_names()` models exactly these names for Career `1..5` while requiring the resource selector as an explicit input.

## Resource refresh path

`MEPlayer::createHeroActionsRes()` at ELF `0x00323174` checks whether the per-player action resource exists. On the missing-resource path it calls `MEPlayer::initActionsSystem()` around `0x0032318e`, then performs the recovered equipment/resource refresh work and unused-texture cleanup.

The currently recovered direct call sites for `createHeroActionsRes()` are in `GameScene::switchNextScene()`. That is evidence for a scene-transition/resource-refresh path, not yet proof that `createHeroActionsRes()` selects the very first offline action/frame after initial construction. The reconstruction therefore does not use this function alone to guess the initial visible player frame.

## Current boundary

The project now has evidence-backed contracts for:

- offline Career -> `MEPlayer` player index;
- singleton first-construction behavior;
- the initial manager point and `SELF` name;
- `loadPlayerData()` -> retained resource-selector state, including the proven `+0x194` value consumed by `createActions()`;
- `setPlayerIdx()` reusing the retained `+0x194` value rather than fetching it itself;
- `pNN_actions.plist` and the nine `pNN_CC_RR_default.plist` names;
- first-scene type-6 event-port selection and exact position transform.

Still unresolved before drawing a faithful first player frame:

- the concrete standalone player-data value written to `MEPlayer + 0x194` for each Career;
- which action is selected immediately after first construction/first scene placement;
- which sprite-frame name that initial action resolves to;
- the minimum input transition required to move from idle into a proven action.

The next bounded #148 task is to trace the initial action-state/frame path from player construction or first scene entry, rather than assuming the transition-only `createHeroActionsRes()` call sites describe first spawn. Once the first frame name is proven, it can be connected to the existing atlas staging/rendering pipeline without placeholder player art.
