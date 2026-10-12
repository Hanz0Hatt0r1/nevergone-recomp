# PVESocketData protocol reconstruction — 2026-10-12

## Scope and provenance

This note records a static reconstruction of the PVE protobuf protocol from the user-owned NeverGone 1.0.9 ARMv7 native library.

- `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`
- APK SHA-256: `c8aaabfe28893998aaf891716147328245e16a043d95b7abdeac43ef6adc9cab`
- embedded generated-source path: `jni/../../Classes/NetSystem/PVESocketData.pb.cc`
- protobuf filename: `PVESocketData.proto`
- package: `PVEDATA`

The original APK and native library are not committed.

## Exact embedded descriptor

`PVEDATA::protobuf_AddDesc_PVESocketData_2eproto()` at ARM/Thumb symbol address `0x0028bec5` registers the generated descriptor with:

- descriptor start: `.rodata` / file offset `0x00767fe3`;
- descriptor size: `0x0d9c` (`3484`) bytes;
- registration call: `google::protobuf::DescriptorPool::InternalAddGeneratedFile(void const*, int)`;
- descriptor SHA-256: `c79ccc34571e60ce4daabd0f45c9eace7e811b6e4305041e2871774b57bc81fc`.

Parsing those exact bytes as `google.protobuf.FileDescriptorProto` yields 19 top-level messages and no imports. The descriptor omits an explicit syntax field, which means proto2 semantics for this generator. The recovered source-compatible schema is stored next to this note as `PVESocketData.proto`.

This descriptor is authoritative for field spelling, field numbers, scalar/message types, and `required` / `optional` / `repeated` labels. This corrects weaker type guesses that can be made from serializer disassembly alone. In particular, several flag-like action fields are `uint32`, and `fbfh` is `float`.

## Message inventory

The 19 recovered messages are:

1. `actorDataMsg`
2. `equipWeaponDataMsg`
3. `equipToolsDataMsg`
4. `equipToolsListMsg`
5. `heroTalentItemData`
6. `heroTalentPageData`
7. `heroTalentDataMsg`
8. `actionDataMsg`
9. `enemyActionDataMsg`
10. `playerActionDataMsg`
11. `enemyHealthDataMsg`
12. `playerHealthDataMsg`
13. `loadingStateDataMsg`
14. `enterEventMsg`
15. `gateInfoMsg`
16. `selectTeamLeaderMsg`
17. `battleResultMsg`
18. `matchSuccessMsg`
19. `BaseData`

`BaseData` is the envelope. Its required field `nMsgId` is field 1, followed by 15 optional payload slots. `tActionData` is field 16.

## Combat payloads

The exact combat-related wire contract is:

- `actionDataMsg`: 17 required fields (`ot`, `sid`, `oid`, `oOnline`, `oHealth`, `enemyClass`, `enemyId`, `os`, `fd`, `x`, `y`, `tx`, `ty`, `fbfh`, `sGotoFrame`, `sGotoFrameIdx`, `isLoading`).
- `enemyActionDataMsg`: 10 required fields.
- `playerActionDataMsg`: 8 required fields.
- `enemyHealthDataMsg`: 3 required fields.
- `playerHealthDataMsg`: 1 required field.
- `battleResultMsg`: 1 required `uint32 nBattleResult` field.

The generated `IsInitialized()` implementations independently confirm the all-required masks for these six messages (`0x1ffff`, `0x3ff`, `0xff`, `0x7`, `0x1`, `0x1`).

## Observed outgoing `nMsgId` assignments

The following IDs are written directly into `BaseData::nMsgId` by native `NetSystem::send*` functions before `SerializeToString`:

| `nMsgId` | Native sender | Payload / meaning |
| ---: | --- | --- |
| 1 | `sendConnentedSuccessData()` | `tMatchSuccessMsg` handshake/player snapshot |
| 2 | `sendActorInfo()` | `tActorInfo` |
| 3 | `sendEquipWeaponInfo()` | `tEquipWeaponData` |
| 4 | `sendEquipTools()` | `tToolsListMsg` |
| 5 | `sendTalentInfo()` | `tHeroTalentData` |
| 6 | `sendGateInfo()` | `tGateInfo` |
| 7 | `sendCancelGate()` | control message |
| 8 | `sendFightStartOrder()` | control message |
| 9 | `sendEnemyActionInfo()` | `tEnemyActionMsg` |
| 10 | `sendPlayerActionInfo()` | `tPlayerActionMsg` |
| 10 | `sendLoadingStateInfo(int)` | `tLoadingStateMsg` (intentional/asymmetric or server-translated reuse must be verified dynamically) |
| 11 | `sendEnemyHarmInfo()` | `tEnemyHealthMsg` |
| 12 | `sendEnemyHealthInfo()` | `tEnemyHealthMsg` |
| 13 | `sendPlayerHealthInfo()` | `tPlayerHealthMsg` |
| 16 | `sendTownEventInfo(int)` | `tEnterEventMsg` |
| 17 | `sendDisconnetOrder()` | control message |
| 18 | `sendConfirmGateEnter()` | control message |
| 19 | `sendPhysicalPower()` | actor/physical-power update |
| 20 | `sendLeaderCampaign()` | `tTeamLeaderMsg` |
| 21 | `sendBattleResult(int)` | `tBattleResultMsg` |
| 22 | `sendHeartBeat()` | heartbeat |
| 23 | `sendChangeClothes()` | actor update |
| 24 | `sendActorUpLevel()` | actor update |

The duplicate outgoing ID 10 is present in the original ARM code: `sendPlayerActionInfo` populates `tPlayerActionMsg`, while `sendLoadingStateInfo` populates `tLoadingStateMsg`. Do not normalize this away until server/peer behavior is proven.

## Main-thread receive dispatch

`NetSystem::MsgUpdata(float)` reads `netBaseData::getMsgID()`, subtracts 1, bounds-checks through ID 26, then dispatches through a Thumb `TBB` jump table. The observed main-thread handlers are:

| ID | Handler |
| ---: | --- |
| 1 | `createTeamPlayer(...)` from the match-success aggregate |
| 2 | `updataActorInfo(...)` |
| 3 | `updataEquipWeaponInfo(...)` |
| 4 | `updataEquipToolsList(...)` |
| 5 | `updataTalentInfo(...)` |
| 6 | `updataFightGateInfo(...)` |
| 7 | `updataCancelGate()` |
| 8 | `startFight()` |
| 14 | `updataOtherLoadingState(...)` |
| 16 | `updataTownEventInfo(...)` |
| 17 | `disconnect()` |
| 18 | `updataConfirmGateEnter()` |
| 19 | `updataTeammatePhysicalPower(...)` |
| 20 | `updataLeaderCampaign(...)` |
| 21 | `updataBattleResult(...)` |
| 22 | `updataHeartBeat()` |
| 23 | `updataChangeClothes(...)` |
| 24 | `updataActorUpLevel(...)` |
| 25 | `sendConnentedSuccessData()` |
| 26 | `onDisconnetClear()` |

IDs 9–13 do not dispatch through the ordinary `MsgUpdata` branch. `NetSystem` also owns actor/enemy message queues and calls `updataActionInfo()` when those queues contain data; this is the next boundary to reconstruct for exact action/health ordering.

## Useful native anchors

- `PVEDATA::protobuf_AddDesc_PVESocketData_2eproto()` — `0x0028bec5`
- `PVEDATA::protobuf_AssignDesc_PVESocketData_2eproto()` — `0x0028c451`
- `PVEDATA::BaseData::SerializeWithCachedSizes(...)` — `0x00286229`
- `NetSystem::sendConnentedSuccessData()` — `0x0030b89d`
- `NetSystem::sendEnemyActionInfo(...)` — `0x0030c6a9`
- `NetSystem::sendPlayerActionInfo(...)` — `0x0030c7b5`
- `NetSystem::sendLoadingStateInfo(int)` — `0x0030ca1d`
- `NetSystem::MsgUpdata(float)` — `0x0030d121`

Symbol values in the ELF have the Thumb low bit set; the disassembly addresses above are displayed with the usual even instruction address.

## Next reconstruction boundary

Use the exact schema and IDs to recover the receive/parser path that constructs `receiveEnemyActionData`, `receivePlayerActionData`, `receiveEnemyHealthData`, and `receivePlayerHealthData`, then map IDs 9–13 into `addActorMsgToList`, `addEnemyMsgToList`, and `updataActionInfo()`. This should provide an evidence-backed bridge from PVE wire messages into `GameScene::olUpdataPlayer2State`, `GameScene::olUpdataEnemyState`, and health updates.
