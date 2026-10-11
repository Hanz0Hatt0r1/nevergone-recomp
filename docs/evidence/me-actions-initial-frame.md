# MeActions initial player frame and attachment evidence

Target: clean-room reconstruction of Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note corrects an earlier reconstruction mistake: the first player action is a `MeActionsSystem`, not an `EnemyActionsSystem`. The two classes consume compatible action-frame records but keep different runtime fields and gates. Player rendering must therefore not reuse `EnemyActionsSystem +0x26c/+0x294` state.

## Initial MeActions frame state

`MeActionsSystem::initWithFile(...)` starts at ELF `0x0032bd10`. Its initialization block writes:

- `0` to `MeActionsSystem + 0x1a4` at `0x0032bd60`;
- `-1` to `MeActionsSystem + 0x1a8` at `0x0032bd64`.

`MeActionsSystem::play()` at ELF `0x0032b5d8` re-establishes the same playback cursor:

- `0x0032b604`: previous frame `+0x1a8 = -1`;
- `0x0032b608`: current frame `+0x1a4 = 0`.

It then loads the action-data object at `+0x110`, reads the primary frame array at action-data `+0x34`, selects `objectAtIndex(+0x1a4)`, copies the selected frame's `+0x5c` value to runtime `+0x1ac`, and calls `MeActionsSystem::updateData()` at `0x0032b68c`.

This proves that the initial `play()` call enters primary frame index **0**. `me_actions_runtime_state::initial_play_state()` models only these two proven fields.

## Frame-name lookup

`MeActionsSystem::updateData()` starts at ELF `0x00329d34`. Both the primary/offline and alternate manager-selection entry branches converge on the same frame-selection body at `0x00329d6e`:

1. load action data from `system + 0x110`;
2. load current frame from `system + 0x1a4`;
3. load the primary frame array from action-data `+0x34`;
4. select `objectAtIndex(current_frame)`;
5. read the selected `ActionFrameData + 0x68` `CCString`;
6. call `CCString::getCString()`;
7. call `CCSpriteFrameCache::spriteFrameByName(...)`.

The project WBG parser already maps this serialized `ActionFrameData +0x68` string to `primary_records[index].third_string`, with the adjacent serialized length retained as `third_string_length_i32`. The player bridge therefore selects the frame key from the parsed primary record indexed by `current_frame_1a4`.

The reconstruction keeps project-owned bounds checks instead of reproducing an invalid `objectAtIndex()` access for malformed state.

## MEPlayer attachment

`MEPlayer::createActions(unsigned int)` stores the selected `MeActionsSystem` at `MEPlayer +0x234`. At ELF `0x003193a0..0x003193ae` it invokes the `MEPlayer` vtable slot at object-vptr `+0xbc` with the selected action node and integer argument `2`.

Resolving the original `CCNode` vtable maps object-vptr slot `+0xbc` to `CCNode::addChild(CCNode*, int)`. Therefore the active `MeActionsSystem` is attached to `MEPlayer` with child Z order **2**. Immediately afterward `MEPlayer::createActions()` calls `MeActionsSystem::play()` and sets the action node position through the inherited `CCNode::setPosition(CCPoint const&)` slot.

This closes the local MEPlayer -> MeActions attachment order. It does not by itself define the final flattened GameScene draw order.

## GameScene / BattleManager parent order

`GameScene::initGameScene()` adds the shared `BattleManager` to `GameScene` through the same `CCNode::addChild(CCNode*, int)` vtable slot with Z order **7**. `BattleManager::addPlayerToBallteManager()` then adds the primary `MEPlayer` under the BattleManager with its recovered per-player integer Z expression.

For the first-player-only renderer boundary, the stable scene-level fact is therefore:

```text
GameScene
  -> BattleManager (z = 7)
      -> MEPlayer
          -> active MeActionsSystem (z = 2)
```

A later flattened renderer integration can insert the player branch after already-added GameScene layer children at Z 7 and before Z 8, preserving same-Z arrival order, but it must still reconstruct the MeActions sprite-local transform/facing behavior before claiming a faithful visual.

## Two-part MEPlayer position

`MEPlayer::initWithMEName(...)` copies the construction point into `MEPlayer +0x240` and initializes `MEPlayer +0x248` to `(0, 0)`.

`MEPlayer::setPlayerPos(CCPoint)` at ELF `0x003187e8` copies its argument into `+0x248`, then computes:

```text
visible_x = player[+0x240].x + player[+0x248].x
visible_y = player[+0x240].y + player[+0x248].y
```

and forwards that summed point to the active `MeActionsSystem::setPosition()`.

`MEPlayer::getPlayerPos()` at ELF `0x00317f78` independently performs the same two additions and returns the summed point.

For the primary offline player, `MEPlayerManager::initMEPlayerManager()` initializes `+0x240` from `(0.5 * win_width, 0.7 * win_height)`. The type-6 `GameScene::loadingTex()` path later passes its transformed entry point to `MEPlayer::setPlayerPos()`, replacing `+0x248` rather than replacing the bootstrap point.

Consequently the previously recovered type-6 transform is a **scene offset**, not the final visible point by itself. `game_scene_player_entry_markers::resolve_offline_player_position()` now composes the proven values and uses the final matching type-6 marker because the original loading loop invokes `setPlayerPos()` for every match in traversal order.

At the recovered 1136x640 design resolution, a raw marker `(600, 300)` produces offset `(32, 20)` and, with bootstrap `(568, 448)`, visible point `(600, 468)`.

## Reconstruction boundary

This change establishes a player-specific initial runtime contract:

- `MeActionsSystem` playback starts at frame index `0`, previous frame `-1`;
- selected primary frame `+0x68` supplies the sprite-frame key;
- active `MeActionsSystem` is a `MEPlayer` child at Z 2;
- the player branch lives under `BattleManager` at GameScene Z 7;
- first-scene visible position is bootstrap `+0x240` plus scene offset `+0x248`.

Still unresolved before drawing the first player frame faithfully:

- the MeActions main-sprite local offset/anchor update applied after `spriteFrameByName()`;
- initial facing/flip behavior for the selected Career/action;
- whether any first-frame equipment/secondary sprites are required for the minimal visible P8 acceptance case;
- live asset staging and renderer attachment for the resulting player sprite request.

No proprietary payload or decompiler-derived source is included.
