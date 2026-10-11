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

The final three factory arguments are instruction-visible: `0`, `1`, `0` are placed on the outgoing stack before the call. `MEPlayer::initWithMEName()` retains the middle value `1` at `MEPlayer + 0x194`, the final value `0` at `+0x198`, and initializes the action-state field at `+0x278` to `0x1b`. If the player index is nonzero it immediately calls `setColorFile(+0x194)`.

These are **construction-time** defaults. They do not prove that scene-visible action resources use selector `1`, because the later data-loading path can replace `+0x194` before `createActions()` runs.

The initial point is likewise an allocation/bootstrap point, not the recovered first-scene entry position. The type-6 `GameScene::loadingTex()` path documented in `player-spawn-metadata-evidence.md` later writes the transformed entry point into the live player-position cursor and calls `MEPlayer::setPlayerPos()`.

`game_scene_player_bootstrap` now retains only the proven construction constants (`+0x194 = 1`, `+0x198 = 0`, action state `0x1b`) plus the initial manager point. It does not reuse selector `1` as a guessed permanent player-resource variant.

## Player-data resource selector flow

`MEPlayer::setPlayerIdx(unsigned int)` at ELF `0x00324e94` stores the player index at `MEPlayer + 0x144`, requests action state `0x1b`, loads the already-retained halfword at `MEPlayer + 0x194`, and tail-calls `MEPlayer::setColorFile(unsigned int)`. It does not perform the player-data lookup itself.

The data-population path is `MEPlayer::loadPlayerData()` at ELF `0x00324eb2`. It obtains the player-data object from the recovered `DataManager + 0x44` path, invokes two virtual getters, stores their returned halfwords at `MEPlayer + 0x194` and `MEPlayer + 0x198`, then calls `setColorFile()` with the `+0x194` value.

`MEPlayer::setColorFile(unsigned int)` starts at ELF `0x003249d8`. It compares its incoming selector against the separate halfword at `MEPlayer + 0x198`; when they differ, the function proceeds through the recovered color/resource-file loading and parsing path using the player index and incoming selector. The current evidence does not justify naming `+0x198` more specifically, nor does this bounded disassembly establish a direct `setColorFile() -> initPlayerRes()` call.

The field at `MEPlayer + 0x194` is independently confirmed as the selector consumed by `MEPlayer::createActions(unsigned int)`, which reads that exact halfword as the second formatter argument for the default sprite plist names.

## Data loading precedes action-resource creation on the recovered standalone scene path

One recovered `GameScene::switchNextScene()` branch provides the critical ordering around ELF `0x00348784..0x003487e6`:

1. obtain the primary `MEPlayer` and call `setPlayerIdx()` with the selected player index;
2. create the standalone `GameSaveData` state;
3. call `DataManager::createData(playerIdx)` at ELF `0x003487e0`;
4. only after `createData()` returns, call `MEPlayer::createHeroActionsRes()` at `0x003487e6`.

`DataManager::createData(unsigned int)` at ELF `0x0029e220` calls `MEPlayer::loadPlayerData()` at `0x0029e2b0` before returning. Therefore on this recovered standalone scene-transition path the player-data values can replace construction-time `+0x194/+0x198` **before** `createHeroActionsRes() -> initActionsSystem() -> createActions()` builds the action resources.

This closes an important ambiguity: construction selector `1` is real, but using `pNN_01_RR_default.plist` as the first scene-visible resource set would still be an unsupported shortcut unless the actual player-data getter result is also proven to be `1`.

## Action and default sprite plist naming

`MEPlayer::initActionsSystem()` loads `MEPlayer + 0x144`, subtracts one, and calls `MEPlayer::createActions(player_idx - 1)` around ELF `0x00322330`.

`MEPlayer::createActions(unsigned int)` at ELF `0x00318d8c` reconstructs the one-based player number and formats resource names with the selector currently retained at `MEPlayer + 0x194`:

```text
p%02d_actions.plist
p%02d_%02d_%02d_default.plist
```

For the default sprite-frame plists the third number iterates from `1` through `9`. The first number is the selected offline Career/player index. The second number is the then-current `+0x194` selector. `%02d` supplies a minimum field width, so the project-owned formatter preserves larger recovered ids rather than inventing a `0..99` range restriction.

`game_scene_player_bootstrap::resource_names()` models exactly these names for Career `1..5` while requiring the resource selector as an explicit input.

## Initial action-state boundary

`MEPlayer::initWithMEName()` initializes `MEPlayer + 0x278` to numeric state `0x1b`. `MEPlayer::setPlayerIdx()` also requests `setActionState(0x1b, true)`.

`MEPlayer::setActionState()` first checks the per-player action array; if no action resource has been created yet, it returns without playing an action. When the array exists, the selected state is used as the index passed to `CCArray::objectAtIndex()`, the resulting `MeActionsSystem` becomes `MEPlayer + 0x234`, and its `play()` path is executed.

This proves numeric state `0x1b` as the initial/requested state, but not yet the sprite-frame name represented by array entry `0x1b`. That frame must come from the actual `pNN_actions.plist`/action-data chain or equivalent runtime evidence.

## Resource refresh path

`MEPlayer::createHeroActionsRes()` at ELF `0x00323174` checks whether the per-player action resource exists. On the missing-resource path it calls `MEPlayer::initActionsSystem()` around `0x0032318e`, then performs the recovered equipment/resource refresh work and unused-texture cleanup.

The recovered direct call sites for `createHeroActionsRes()` are in `GameScene::switchNextScene()`. At least one standalone scene-transition path calls `DataManager::createData()` first, as described above. `createHeroActionsRes()` itself does not call `setActionState()` after building the action array, so the remaining first-visible-frame question still requires the subsequent action/update path or action data itself.

## Current boundary

The project now has evidence-backed contracts for:

- offline Career -> `MEPlayer` player index;
- singleton first-construction behavior;
- the initial manager point and `SELF` name;
- construction-time `MEPlayer +0x194 = 1`, `+0x198 = 0`, and action state `0x1b`;
- `loadPlayerData()` as the path that can replace `+0x194/+0x198` from player data;
- a recovered standalone `switchNextScene()` ordering where `createData()`/`loadPlayerData()` precede action-resource creation;
- `pNN_actions.plist` and the nine `pNN_CC_RR_default.plist` names parameterized by the then-current `+0x194` selector;
- first-scene type-6 event-port selection and exact position transform.

Still unresolved before drawing a faithful first player frame:

- the concrete standalone player-data getter value written to `MEPlayer + 0x194` for each Career;
- which path first re-enters `setActionState(0x1b, ...)` after the action array exists, or equivalently which action-data entry supplies the first visible frame;
- the sprite-frame name resolved by action array entry `0x1b`;
- the minimum input transition required to move from that initial state into a proven movement/action state.

The next bounded #148 task is therefore to recover the `DataManager +0x44` player-data getter behind `+0x194` and/or trace action entry `0x1b` into its concrete frame name. Either route can then feed the existing atlas staging/rendering pipeline without placeholder player art.
