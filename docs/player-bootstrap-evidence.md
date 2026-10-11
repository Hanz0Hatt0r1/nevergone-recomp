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

## Player index selects player data, which selects the color/resource variant

`MEPlayer::setPlayerIdx(unsigned int)` at ELF `0x00324e94` stores the player index at `MEPlayer + 0x144`, requests player data using `player_idx - 1`, obtains a value through the recovered player-data virtual getter, and passes that value to `MEPlayer::setColorFile(unsigned int)`.

`MEPlayer::setColorFile(unsigned int)` at ELF `0x0032497c` compares the incoming value with `MEPlayer + 0x194`. When it changes, the value is stored and `initPlayerRes()` is invoked before the relevant action/resource state is reset.

Therefore `MEPlayer + 0x194` is an evidence-backed color/resource variant id. Its actual value must come from player data. The reconstruction must not invent a default such as zero or one merely because formatted file names are easier to construct that way.

## Action and default sprite plist naming

`MEPlayer::initActionsSystem()` loads `MEPlayer + 0x144`, subtracts one, and calls `MEPlayer::createActions(player_idx - 1)` around ELF `0x00322330`.

`MEPlayer::createActions(unsigned int)` at ELF `0x00318d8c` reconstructs the one-based player number and formats resource names with the retained color/resource variant:

```text
p%02d_actions.plist
p%02d_%02d_%02d_default.plist
```

For the default sprite-frame plists the third number iterates from `1` through `9`. The first number is the selected offline Career/player index. The second number is the player-data-derived color/resource variant id. `%02d` supplies a minimum field width, so the project-owned formatter preserves larger recovered ids rather than inventing a `0..99` range restriction.

`game_scene_player_bootstrap::resource_names()` models exactly these names for Career `1..5` while requiring the color/resource variant as an explicit input.

## Current boundary

The project now has evidence-backed contracts for:

- offline Career -> `MEPlayer` player index;
- singleton first-construction behavior;
- the initial manager point and `SELF` name;
- player-index -> player-data lookup -> color/resource variant;
- `pNN_actions.plist` and the nine `pNN_CC_RR_default.plist` names;
- first-scene type-6 event-port selection and exact position transform.

Still unresolved before drawing a faithful first player frame:

- which player-data field/loader supplies the concrete color/resource id for each Career in standalone startup;
- which action is selected immediately after first construction/first scene placement;
- which sprite-frame name that initial action resolves to;
- the minimum input transition required to move from idle into a proven action.

The next bounded #148 task is therefore to trace the first `createHeroActionsRes` / action-selection path after offline scene initialization and connect the resulting frame name to the existing atlas staging/rendering pipeline. No placeholder player art is required for that evidence step.
