# Player-start and player-entry position evidence

This note records the current evidence boundary for issue #148. It deliberately separates scene-list metadata, binary scene entry markers, and the live player-position state.

## Proven scene-list start point

The metadata-only query in `tools/ghidra/player_spawn_evidence.py` derives the Ghidra-to-ELF load delta from `GameScene::loadGameSceneList(char const*)` instead of hard-coding it. The checked-in Nevergone index produces a delta of `0x00010000`.

The two scene-list keys have exact Ghidra string references inside `GameScene::loadGameSceneList(char const*)`:

| Key | String address | Ghidra reference | ELF code offset |
| --- | ---: | ---: | ---: |
| `playerStartXPos` | `0x007821df` | `0x003512da` | `0x003412da` |
| `playerStartYPos` | `0x007821ef` | `0x003512fa` | `0x003412fa` |

The resolved calls immediately following those references establish the scalar conversion used by the original loader:

- X: `CCDictionary::objectForKey` at Ghidra `0x003512e4`, then `CCString::floatValue` at `0x003512e8`;
- Y: `CCDictionary::objectForKey` at `0x00351304`, then `CCString::floatValue` at `0x00351308`.

A bounded Thumb-2 disassembly check against the user-owned 1.0.9 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`) closes the previous call-proximity ambiguity:

- the X `CCString::floatValue` result at ELF `0x003412e8` is stored at stack offset `+0x18`;
- the Y result at ELF `0x00341308` is stored at stack offset `+0x1c`;
- at ELF `0x003413b2` and `0x003413b6`, those exact stack words are loaded into `r1` and `r2` while `r0` points at a temporary `CCPoint`;
- the call at ELF `0x003413b8` is `cocos2d::CCPoint::CCPoint(float, float)`;
- immediately afterward, `r0` is formed as `GameLevelsData + 0x34`, the temporary point is passed in `r1`, and `CCPoint::operator=` is called at ELF `0x003413c2`.

Therefore `playerStartXPos` and `playerStartYPos` are evidence-backed floating-point values that form a `CCPoint` retained in `GameLevelsData + 0x34`.

`GameSceneAtlasListDiscovery.Result` retains both values as finite scene-entry metadata. `GameSceneStartPoint` is the project-owned typed representation of that proven pair. This alone still does not prove that `GameLevelsData + 0x34` is the first offline player transform.

## Proven type-6 entry-position selection

The binary GameLevels parser and `GameScene::loadingTex()` provide a separate, direct player-placement path.

For object type code `6`, `GameLevels::LoadGL_Scene()` reads the recovered one-`int32` type tail at ELF `0x002c2ce0` and stores that exact value into `GameSceneLayerObjectData + 0x58` at ELF `0x002c2db6`. The same record's serialized first point is retained at object-data offset `+0x1c`.

During `GameScene::loadingTex()`:

1. only the first 11 ordered scene layers are visited, matching the already recovered construction traversal;
2. each layer object is inspected in source order;
3. an object enters the special path only when `GameSceneLayerObjectData + 0x14 == 6`;
4. its `+0x58` value is compared against `GameLevels + 0x40` at ELF `0x003467a2..0x003467ac`;
5. `GameLevels + 0x40` is independently proven by `GetPortNodeLinkPortNode(EVENT_PORT_TYPE)` to be the retained numeric **event-port type**;
6. on a match, `loadingTex()` copies the object's point at `+0x1c`, applies the original scene/window transform, assigns the transformed point into `GameScene + 0x4c4`, and calls the primary `MEPlayer::setPlayerPos()` at ELF `0x00346866`.

This establishes a real first-scene player-placement contract: type-6 objects are filtered by the current event-port type and matching objects drive `MEPlayer::setPlayerPos()`. Multiple matches are not collapsed to a guessed unique marker; the original loop executes the path for each match in traversal order.

`game_scene_player_entry_markers::collect()` encodes only the proven pre-transform selection boundary. It visits the first 11 layers, preserves object order, selects type `6`, compares `tail_i32` bit-for-bit with the retained event-port type, and returns the raw serialized points. Missing type tails or non-finite matching coordinates fail closed.

## Proven live player-position cursor

`GameScene::adjustPlayerPos(int)` is also resolved directly from ARMv7 instructions; the integer argument is not a spawn-selector enum.

For a nonzero argument the function:

1. converts the integer to float;
2. adds it to the X component of the `CCPoint` at `GameScene + 0x4c4`;
3. obtains the primary player from `MEPlayerManager`;
4. calls `MEPlayer::movePlayer(GameScene + 0x4c4)`.

A zero argument is a no-op.

The same `GameScene + 0x4c4` point is synchronized from the primary player's point at `MEPlayer + 0x248` by `GameScene::updatePlayerPos()`. `GameScene::setPlayerPosAddOffset(CCPoint)` likewise starts from `MEPlayer + 0x248`, adds the requested offset, writes the result to `GameScene + 0x4c4`, and calls `MEPlayer::setPlayerPos()`.

Together with the type-6 `loadingTex()` path, this proves that `GameScene + 0x4c4` is live player-position state used for initial placement and subsequent horizontal adjustment.

## Current project-owned boundary

The project now has two separately proven position inputs:

- `GameSceneStartPoint`: the scene-list `playerStartXPos/YPos` pair stored by the original in `GameLevelsData + 0x34`;
- `game_scene_player_entry_markers`: ordered type-6 binary scene objects whose type tail matches the current event-port type and whose point enters the original `MEPlayer::setPlayerPos()` path.

They must not be conflated. No direct value-flow edge has yet been proven from `GameLevelsData + 0x34` to `GameScene + 0x4c4` or to the type-6 object point.

The safe next implementation boundary for #148 is therefore narrower than before: reproduce the proven offline transform from a selected type-6 object's raw `+0x1c` point into the `GameScene + 0x4c4` coordinate space, then attach a minimal project-owned player visual/input state to that transformed point. Player resource selection still requires its own evidence.

## Reproduction

The checked-in metadata evidence can be reproduced without the original APK/OBB or Ghidra project:

```bash
python tools/ghidra/player_spawn_evidence.py
python tools/ghidra/query_full_index.py callees GameScene::loadGameSceneList --limit 100
```

The instruction-level observations above come from the user-owned binary identified by hash. Only derived addresses, field offsets, register/stack flow, and behavioral contracts are retained in the repository; the original binary remains outside Git.
