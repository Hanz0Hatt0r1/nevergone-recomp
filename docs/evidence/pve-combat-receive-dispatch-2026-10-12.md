# PVE combat receive/dispatch reconstruction — 2026-10-12

## Scope and provenance

Static reconstruction from the user-owned NeverGone 1.0.9 ARMv7 `libcocos2dcpp.so`.

- native SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`
- protobuf schema: [`PVESocketData.proto`](PVESocketData.proto)
- parser entry reached by `NetSystem::recieveCallBack(std::string*)`: `0x0030ab00`
- main-thread combat dispatcher: `NetSystem::updataActionInfo()` at `0x0030a110`
- main update: `NetSystem::MsgUpdata(float)` at `0x0030d120`

The original native library is not committed.

## Receive parser

`NetSystem::recieveCallBack(std::string*)` at `0x0030b888` first rejects an empty string and a disabled connection state. The accepted path branches to `0x0030ab00`.

The parser block:

1. constructs `PVEDATA::BaseData` on the stack;
2. calls `google::protobuf::MessageLite::ParseFromString` at `0x0030ab16`;
3. reads `BaseData::nMsgId` from the native generated-message slot corresponding to field 1;
4. dispatches IDs 1..24 through a Thumb `TBH` table at `0x0030ab24`.

For combat traffic, the exact case entries are:

| ID | Parser target | Required `BaseData` payload | Receive wrapper | Queue |
| ---: | ---: | --- | --- | --- |
| 9 | `0x0030b386` | `tEnemyActionMsg` | `receiveEnemyActionData` | enemy |
| 10 | `0x0030b452` | `tPlayerActionMsg` | `receivePlayerActionData` | actor |
| 11 | `0x0030b50c` | `tEnemyHealthMsg` | `receiveEnemyHealthData` | enemy |
| 12 | `0x0030b50c` | `tEnemyHealthMsg` | `receiveEnemyHealthData` | enemy |
| 13 | `0x0030b58c` | `tPlayerHealthMsg` | `receivePlayerHealthData` | actor |
| 14 | `0x0030b5f2` | `tLoadingStateMsg` | `receiveLoadingStateData` | ordinary message list |

The parser preserves the original `nMsgId` in each `netBaseData`-derived receive wrapper. This is important for IDs 11 and 12: both carry the same protobuf payload type, and the later main-thread dispatcher distinguishes their semantics by message ID.

## Protobuf -> clean-room combat-data conversion

The native parser converts protobuf fields into the smaller `OL*` transport structs before queueing the receive wrapper. This conversion is not a byte-for-byte protobuf copy; several 32-bit integer fields are intentionally narrowed by the original game.

### ID 9: `enemyActionDataMsg` -> `OLEnemyActionData`

Parser block: `0x0030b386..0x0030b450`.

| Protobuf field | Proto type | Native `OLEnemyActionData` storage |
| --- | --- | --- |
| `sid` | `int32` | 16-bit |
| `os` | `uint32` | 16-bit |
| `fd` | `uint32` | 8-bit |
| `x` | `float` | 32-bit float |
| `y` | `float` | 32-bit float |
| `oid` | `uint32` | 16-bit |
| `tx` | `float` | 32-bit float |
| `ty` | `float` | 32-bit float |
| `enemyId` | `uint32` | 16-bit |
| `oHealth` | `float` | 32-bit float |

The receive object is allocated with size `0x30`, initialized by `receiveEnemyActionData::receiveEnemyActionData()`, receives the original message ID through the virtual `netBaseData` setter, receives the converted action value through its data setter, and is pushed with `NetSystem::addEnemyMsgToList()` at `0x0030b446`.

### ID 10: `playerActionDataMsg` -> `OLPlayerActionData`

Parser block: `0x0030b452..0x0030b50a`.

| Protobuf field | Proto type | Native `OLPlayerActionData` storage |
| --- | --- | --- |
| `sid` | `int32` | 16-bit |
| `os` | `uint32` | 16-bit |
| `fd` | `uint32` | 8-bit |
| `x` | `float` | 32-bit float |
| `y` | `float` | 32-bit float |
| `fbfh` | `float` | 32-bit float |
| `sGotoFrame` | `int32` | 16-bit |
| `sGotoFrameIdx` | `int32` | 16-bit |

The receive object is allocated with size `0x24` and pushed with `NetSystem::addActorMsgToList()` at `0x0030b500`.

### IDs 11/12: `enemyHealthDataMsg` -> `OLEnemyHealthData`

Parser block: `0x0030b50c..0x0030b58a`.

| Protobuf field | Proto type | Native `OLEnemyHealthData` storage |
| --- | --- | --- |
| `oHealth` | `float` | 32-bit float |
| `sid` | `int32` | 16-bit |
| `oid` | `uint32` | 16-bit |

The receive object is allocated with size `0x14` and pushed to the enemy queue. IDs 11 and 12 deliberately share this parser target.

### ID 13: `playerHealthDataMsg` -> `OLPlayerHealthData`

Parser block: `0x0030b58c..0x0030b5f0`.

`oHealth` remains a 32-bit float. The receive object is allocated with size `0x10` and pushed to the actor queue.

## Thread-safe queue boundary

There are separate input and read/snapshot vectors for actor and enemy traffic.

### Producer side

- `NetSystem::addActorMsgToList()` at `0x0030a7c4`
  - locks `ThreadMutex` at `NetSystem + 0x21c`;
  - pushes into actor staging vector at `NetSystem + 0x1e4`;
  - unlocks.
- `NetSystem::addEnemyMsgToList()` at `0x0030a7ec`
  - uses the same mutex;
  - pushes into enemy staging vector at `NetSystem + 0x1f0`;
  - unlocks.

### Consumer side

`NetSystem::MsgUpdata(float)` first clears the prior read lists, then locks the same mutex. It copies pointers from:

- actor staging `+0x1e4` -> actor read list `+0x1fc`;
- enemy staging `+0x1f0` -> enemy read list `+0x208`.

After the transfer, it clears/deletes the old staging ownership and unlocks. `NetSystem::updataActionInfo()` consumes the read lists on the main thread.

This establishes a concrete network-thread -> main-thread handoff rather than direct scene mutation from the receive callback.

## Main-thread combat dispatch

`NetSystem::updataActionInfo()` at `0x0030a110` walks the actor read list first, then the enemy read list.

### Actor read list (`NetSystem + 0x1fc`)

| ID | Receive value | Scene call |
| ---: | --- | --- |
| 10 | `OLPlayerActionData` | `GameScene::olUpdataPlayer2State(...)` at `0x00343ad4` |
| 13 | `OLPlayerHealthData` | `GameScene::olUpdataPlayer2Health(...)` at `0x003409fc` |

### Enemy read list (`NetSystem + 0x208`)

| ID | Receive value | Scene call |
| ---: | --- | --- |
| 9 | `OLEnemyActionData` | `GameScene::olUpdataEnemyState(...)` at `0x00340aa8` |
| 11 | `OLEnemyHealthData` | `GameScene::olUpdataEnemyHealth(..., 1)` at `0x00340a1e` |
| 12 | `OLEnemyHealthData` | `GameScene::olUpdataEnemyHealth(..., 2)` at `0x00340a1e` |

Thus IDs 11 and 12 are not duplicates at the gameplay layer: they intentionally select mode `1` versus mode `2` in `GameScene::olUpdataEnemyHealth`.

## Reconstructed end-to-end combat path

The evidence-backed receive path is now:

```text
network string
  -> NetSystem::recieveCallBack
  -> PVEDATA::BaseData::ParseFromString
  -> ID 9..13 parser case
  -> protobuf payload -> OL* value
  -> receive*Data wrapper with original nMsgId
  -> actor/enemy staging vector under ThreadMutex
  -> NetSystem::MsgUpdata copies to read vector
  -> NetSystem::updataActionInfo
  -> GameScene::olUpdata*()
```

## Next reconstruction boundary

The next high-value boundary is inside the four `GameScene::olUpdata*` consumers. Reconstructing their field-by-field effects will connect the now-exact PVE wire format to enemy/player state machines, animation selection, hit reactions and health presentation without guessing from names alone.
