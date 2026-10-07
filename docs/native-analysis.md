# Native analysis baseline

The initial native target is `lib/armeabi-v7a/libcocos2dcpp.so` from Never Gone 1.0.9.

## Why this binary is unusually workable

Although normal debug information is absent, the shared library retains a very large exported/dynamic symbol set. An automated pass with `readelf`, `c++filt`, and `tools/native_symbol_map.py` finds approximately **26,875 defined function symbols**.

This means much of the first-stage class/function naming can be recovered without guessing from decompiler output.

## Initial category counts

The current heuristic classifier reports approximately:

| Category | Functions |
| --- | ---: |
| game / other third-party | 15,262 |
| Cocos2d-x | 7,422 |
| Google protobuf | 2,250 |
| C++ standard library | 917 |
| Lua API / bindings | 572 |
| TinyXML | 235 |
| runtime / low-level | 195 |
| JNI | 22 |

The classifier is deliberately conservative. `game-or-third-party` contains both real game code and libraries not yet split into dedicated categories.

## High-value named game classes

Large symbol groups include:

| Class / namespace | Approx. functions |
| --- | ---: |
| `PVEDATA` | 461 |
| `MEPlayer` | 262 |
| `GameSceneUI` | 169 |
| `GameScene` | 144 |
| `achieveHonorManager` | 100 |
| `EnemyObject` | 99 |
| `WeaponsItemData` | 71 |
| `NetSystem` | 69 |
| `DataManager` | 64 |
| `GuideManager` | 63 |
| `BattleManager` | 56 |
| `LansquenetObject` | 56 |
| `MeActionsSystem` | 56 |
| `MEWarehouse` | 55 |
| `AppParameters` | 54 |
| `EnemyActionsSystem` | 53 |
| `actorData` | 51 |
| `GameSaveData` | 50 |

This is already enough to build a dependency-first reconstruction queue around startup, scene state, player state, save data, UI, and combat.

## JNI map

Addresses below have the ARM Thumb low bit cleared for analysis-tool convenience.

| Address | Export |
| --- | --- |
| `0x00284698` | `JNI_OnLoad` |
| `0x002846a8` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit` |
| `0x002c337c` | `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnFailed` |
| `0x002c351c` | `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnPurchased` |
| `0x002c35cc` | `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnRestore` |
| `0x002c3820` | `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnReceiveItemInfo` |
| `0x003d9610` | `Java_org_cocos2dx_lib_Cocos2dxHelper_SendInfo` |
| `0x005359fc` | `Java_org_cocos2dx_lib_Cocos2dxBitmap_nativeInitBitmapDC` |
| `0x00535bdc` | `Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetApkPath` |
| `0x00535c04` | `Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetEditTextDialogResult` |
| `0x0053663c` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender` |
| `0x0053664c` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause` |
| `0x00536670` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume` |
| `0x00536686` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInsertText` |
| `0x005366c0` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeDeleteBackward` |
| `0x005366d0` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeGetContentText` |
| `0x00536710` | `Java_org_cocos2dx_lib_Cocos2dxAccelerometer_onSensorChanged` |
| `0x005369cc` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesBegin` |
| `0x005369ec` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesEnd` |
| `0x00536a0c` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesMove` |
| `0x00536a94` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeTouchesCancel` |
| `0x00536b1c` | `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeKeyDown` |

## Lua-related native evidence

The library embeds a full Lua runtime and exposes many Lua-facing game classes/functions. Examples include `CallLuaCalss`, `lua_ProtoRPC`, `LUA_LOGIN`, `LUA_WAREHOUSE`, `LUA_CHAPTR`, and gameplay methods such as `EnemyObject::execLUAScript()`.

`CallLuaCalss::init()` performs the following observable sequence:

1. initialize its Cocos2d-x layer base;
2. obtain `cocos2d::CCFileUtils::sharedFileUtils()`;
3. resolve the path corresponding to `assets/Script/Game/StartLua.lua`;
4. obtain the global Lua state;
5. call the embedded `luaL_loadfilex`;
6. call `lua_pcallk` if loading succeeds.

Because the APK copy of `StartLua.lua` is not normal Lua text/bytecode, the critical research question is what transforms the stored bytes before or during the file-reader path used by `luaL_loadfilex`.

## Encryption-related symbols

The binary exposes:

- `IsEncryptFile(unsigned char*, int)` at approximately `0x0030f748`
- `DecryptMemory(unsigned char*, int, char const*, int)` at approximately `0x0030f770`
- `EncryptMemory(unsigned char*, int, char const*, int)` at approximately `0x0030f7c8`

Thumb disassembly indicates that `DecryptMemory` applies a repeating-key XOR to the first 256 bytes and then bitwise-inverts bytes; `EncryptMemory` performs the inverse ordering. However, currently identified direct calls occur in payment/socket routines. This transform should be documented and tested, but it is not yet evidence of the script codec.

Other protocol encryption classes (`hProtocolEncrypt*`) also exist and appear to belong to networking.

## Recommended Ghidra workflow

1. Import `libcocos2dcpp.so` as ARM little-endian ELF.
2. Preserve the existing ELF symbol names; do not discard them in favor of autogenerated names.
3. Apply Thumb mode where indicated by odd symbol addresses.
4. Use the CSV generated by `tools/native_symbol_map.py` as a searchable subsystem index.
5. Begin with `JNI_OnLoad`, `CallLuaCalss::init`, `CCFileUtils` call sites, and game startup singletons before decompiling large combat/UI classes.
6. Tag upstream Cocos2d-x/protobuf/TinyXML functions early so decompiler effort stays focused on project-specific code.

## Reproducing the symbol map

```bash
python3 tools/native_symbol_map.py /path/to/libcocos2dcpp.so \
  --csv build/native-symbols.csv \
  --markdown build/native-symbols.md
```

The CSV includes raw and Thumb-bit-cleared addresses, symbol sizes, mangled/demangled names, heuristic category, and top-level owner/class.
