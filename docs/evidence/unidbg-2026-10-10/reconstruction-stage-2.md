# Dynamic reconstruction, stage 2 (2026-10-10)

Original `libcocos2dcpp.so` SHA-256 before and after this run:
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
ELF32 ARM Thumb symbol values carry bit 0; instruction offsets below clear it.
The checked-in Ghidra call index sometimes uses an image address 0x10000 above the ELF symbol value (for example its GameSaveData::Encode entry is 0x34a766 while the ELF code offset is 0x33a766). Offsets here were checked against the ELF symbol table and Thumb disassembly.

## GameSaveData graph and probe choice

The [related symbol inventory](save-related-symbols.tsv) contains 144 defined dynamic-function rows, including every `GameSaveData` export and selected DataManager save/load, CCFileUtils, CCUserDefault and protobuf serialization functions. It includes constructor/destructor aliases; row count is not unique behavior. Relevant exported code offsets: `Encode` 0x33a766, `Decode` 0x33a788, `getFileData` 0x33a7aa, `isCreated` 0x33b090, `checkSaveFileData` 0x33ae24, `initGameSaveData` 0x33b6cc, `write` 0x33be38, `read(char*,unsigned,unsigned long)` 0x33e4a4, `read(std::string,unsigned)` 0x33e690, `createGameSaveData` 0x33d168. `DataManager::loadGameSaveData` is 0x29d110. CCUserDefault methods are exported around 0x53d02c..0x53d154; the inspected direct GameSaveData edges do not call them.

Observed direct call edges from the checked-in Ghidra index, with disassembly checks on the leaf routines:

```text
DataManager::loadGameSaveData -> GameSaveData::createGameSaveData
GameSaveData::getFileData -> GameSaveData::Decode
GameSaveData::checkSaveFileData -> GameSaveData::getFileData, Encode,
    CCFileUtils::sharedFileUtils, CCString::createWithFormat, readDecodeSaveDataL
GameSaveData::write -> Encode, CCFileUtils::sharedFileUtils,
    MEPlayerManager::sharedMEPlayerManager, lua_LGGDevice::getDeviceUUID
GameSaveData::isCreated -> CCFileUtils::sharedFileUtils, CCString::createWithFormat,
    virtual file-existence call through FileUtils vtable +0x68
GameSaveData::initGameSaveData -> CCArray::createWithCapacity, CCObject::retain
```

The two leaf transforms have no calls. Thumb disassembly shows `r0` (`this`) unused, `r1` source, `r2` byte length, `r3` destination, and the fifth argument (uint32 key) at the caller stack. `Encode` writes `((source[i] + counter) xor key) & 255`; `Decode` writes `((source[i] xor key) - counter) & 255`. Counter advances modulo 127. Both return length in `r0`. They need no constructed GameSaveData object. `getFileData`, file checks, full read/write and initialization require file paths, C++ strings, arrays, ownership or wider game state and were not invoked.

[save-probes.jsonl](save-probes.jsonl) contains 36 native calls: both directions, keys 0/1/0x123, lengths 0/1/126/127/128/255. Every output matched `verify_save_probes.py`, returned the length, preserved source and the 0x5a destination guard. The runner records `registers_before_call` and `registers_after_call`; the former is the emulator state *before* unidbg sets up the new call, not entry registers. No instruction trace was collected (`trace.mode=none`). The target build loaded 48 modules with zero unresolved symbols and explicit JNI_OnLoad; constructors remained disabled. These cases do not validate malformed pointers, negative lengths, overlapping buffers or arbitrary key widths.

## EnemyActionsData: static dependency reconstruction

`EnemyActionsData::loadWBGFile(CCString*)` is at **0x28f430** (Thumb symbol value 0x28f431; size 4108 bytes). Its entry saves `r0=this` and `r1=CCString*`, calls `AppParameters::sharedAppParameters` and `CCFileUtils::sharedFileUtils`, then `CCString::getCString`. It calls a FileUtils virtual method through vtable slot `+0x18` to form a path, creates another CCString, and calls `HPData::createWithContentsOfFile` at 0x2c652c. A null HPData branches to the end at 0x290422. Non-null data is repeatedly read with `HPData::getBytes` variants (0x2c658a, 0x2c65ce, 0x2c65ac, 0x2c65f0, 0x2c6568); the parser then creates CCStrings, ActionFrameData, retains objects, and adds them to CCArray. This is an actual file/parser dependency, not a standalone string decoder.

The constructor at 0x28f394 zeroes pointer slots `this+0x14..0x30`, `+0x34..0x80`, and `+0x84..0xa0`, plus `+0xc8/+0xcc`. `initWithFile` at 0x29043c writes scalar defaults at `+0xa4..0xd4`, allocates and retains multiple CCArray fields at the pointer offsets, initializes 100 float/zero pairs starting at `+0xd8/+0x268`, then calls `loadWBGFile` at 0x29054a. At parser entry, early HPData reads target `this+0xd4`, `+0xd0`, `+0xc4`. These offsets are observed writes and do not establish a complete class layout. FileUtils vtable dispatch, HPData's bounds/format rules, and CCString/CCArray ownership remain unresolved. Consequently **no WBG probe was run**; a fake pointer or arbitrary bytes would not be a safe test.

## Lua binding sample

The [lua.tsv](lua.tsv) inventory has 596 selected rows, but no export named GameSaveData or EnemyActionsData. The checked-in call index and Thumb disassembly support these representative paths; none is claimed as a direct binding to those two classes.

| Lua-facing export (instruction offset) | C++ target seen in call graph | Argument conversion seen in disassembly | Return conversion seen |
|---|---|---|---|
| `lua_system::Lua_GetSaveFilePath(lua_State*)` 0x4ca15c | `lua_system::GetSaveFilePath` 0x4ca108 | no Lua value read; passes local `std::string` return storage | calls `lua_pushstring` 0x4afd28 with string data, destroys temporary, returns 1 |
| `LUA_LOGIN::cpp_OnCreateTheRole(lua_State*)` 0x2e8ce0 | `JsonDataManage::createWithJson` 0x2ef760; `ManagementLayer::sharedManagementLayer` 0x2f804c; `SaveDataHero::operator=` 0x2e20b6; success callback 0x2e8a78 | `lua_tolstring(L,1,NULL)` 0x4afb80, constructs `CCString` 0x519db4, parses JSON | returns integer 1 after side effects; no direct `lua_push*` in this wrapper, so stack result semantics need live Lua validation |
| `LUA_CHAPTR::cpp_OnChapterCompleteSuccess(lua_State*)` 0x2e6ec8 | `GameSceneUI::sharedGameSceneUI` 0x3d95e0; `OL_ShowOverPane` 0x3d96c8 | `lua_tolstring(L,1,NULL)` then CCString and JSON parse | calls UI method with `true`, returns integer 1; no direct value push observed |
| `cpp_OnUpdateData(lua_State*)` 0x2faf94 | dispatcher includes `LUA_WAREHOUSE::lua_GetSaveDataAllChallengeMode` 0x2ee120, which calls `DataManager::getProgressRecordDataOL` 0x29f610 | first argument read with `lua_tolstring(L,1,NULL)`, then CCString/JSON parsing; branch on decoded command string | branch-specific return conversion unresolved; no direct GameSaveData target established |

For EnemyActionsData, the call index instead links `ActionDataManager::addActionDataWithID` and `EnemyActionsSystem::initWithFile` to `createWithFile`; no Lua export directly calls `loadWBGFile` in the inspected index. GameSceneUI is the direct scene-related target in the table; `GameScene` itself is reached in `LUA_SENDAUCTION::OnSendAuction`, which is not a `lua_State*` export. This distinction prevents treating all Lua-named methods as bindings.

## Reproduce

From `tools/unidbg`, with original libraries and matching local Android ARM32 dependencies configured as in its README:

```sh
python3 save_probe_plan.py target/save-plan.json
./run.sh --probe-plan target/save-plan.json > target/save-probe.log 2>&1
python3 verify_save_probes.py target/save-probe.log
```

The runner accepts only the two audited GameSaveData symbols at their exact offsets; it rejects arbitrary native calls. It records `EVIDENCE` JSON lines in the log. To compare with the committed evidence, extract those lines without the prefix. Source library hashes are checked by RELR preparation and should be rechecked with `sha256sum` after a run. EGL is loadable but uninitialized. No renderer or constructor probe was attempted.

Actual local checks: Java harness packaged successfully; `python3 -m unittest discover -s tools/unidbg -p 'test_*.py'` ran 7 tests, all passed; `verify_save_probes.py` passed 36 native cases; `verify_evidence.py` passed the prior 100 decoder comparisons, 100 guards and 12 geometry observations. The five original source-library SHA-256 hashes matched the earlier inventory after the run. No emulator fault or exception was found in the new probe log.
