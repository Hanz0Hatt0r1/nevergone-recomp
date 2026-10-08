# Lua → native API map

The client Lua layer is now cross-referenced against the shipped ARMv7 `libcocos2dcpp.so`. This turns the script/native boundary from a broad reverse-engineering target into a finite list of binding names that can be reconstructed incrementally.

The analysis is produced by `tools/lua_native_api_map.py`. It decodes Lua only in memory, reuses the static `Game.StartLua` reachability graph, extracts the original native library from a user-supplied APK, demangles its dynamic symbols, and checks native string evidence. No Lua source or proprietary binary data is written into the report.

## Known APK baseline

For the known original Android APK:

| Metric | Result |
| --- | ---: |
| Lua modules | 107 |
| Modules reachable from `Game.StartLua` | 101 |
| Modules analyzed by default | 101 |
| Native-shaped/evidenced API candidates | 117 |
| Candidates with native ELF evidence | 108 |
| Dynamic-symbol matches | 106 |
| Native binary-string matches | 1 |
| Native-class symbol matches | 1 |
| Native-shaped names without current ELF evidence | 9 |

The single class-level match is `ProtoRPC:new`, backed by the exported `lua_ProtoRPC` class and its methods. The string-only confirmed name is `Lua_GetDeviceUUID`; it is present in the native binary even though it is not exposed as a dynamic C++ symbol.

## Immediate startup contract

Only seven native-facing calls are made directly by `Game.StartLua` in the current static scan:

| Lua-facing name | Native evidence | Role |
| --- | --- | --- |
| `CAddDoString` | dynamic symbol | load/execute the two startup aggregator scripts |
| `Lua_GetPlatformString` | dynamic symbol | select platform-specific Lua behavior |
| `Lua_GetDeviceUUID` | binary registration/string evidence | device identifier used by login/startup logic |
| `cpp_ShowErrorDialogUI` | dynamic symbol | error UI bridge |
| `cpp_ShowLoadingUI` | dynamic symbol | loading UI bridge |
| `cpp_HideLoadingUI` | dynamic symbol | loading UI bridge |
| `cpp_ShowMessageBoxUI` | dynamic symbol | message-box bridge |

This is the first practical native binding milestone for the recompilation. The modern runtime does **not** need to reimplement all 117 candidates before executing `Game.StartLua`; it needs the startup contract first, followed by bindings demanded by the next reachable behavior.

`CAddDoString` is especially important: it accounts for roughly one hundred static script-loader calls across the normal client graph and is already preserved as a named dynamic symbol in the original library.

## High-value confirmed APIs after startup

The cross-check also confirms a number of infrastructure bindings that appear in reachable client modules:

```text
Lua_GetPathWithFileName
Lua_GetSetFilePath
Lua_SetConsoleColor
Lua_CopyFile
cpp_OnReceivedChatMessages
cpp_connect_pve
cpp_OnGetServerList
cpp_OnUpdateData
```

Many gameplay/network callbacks also map directly to named methods on classes such as:

```text
LUA_LOGIN
LUA_MAIL
LUA_GUILD
LUA_FRIEND
LUA_MERCENARY
LUA_WAREHOUSE
NetSystem
```

Because these methods retain dynamic C++ symbol names, they can be grouped and reconstructed by subsystem instead of identified manually from anonymous ARM functions.

## Native-shaped calls not yet confirmed by ELF metadata

Nine reachable names have native-style naming but are not currently matched to a dynamic symbol or exact registration string:

```text
cpp_StartBattle
LGG_IsFileExist
Lua_CheckNickName
Lua_CheckStringLegal
Lua_GetBundleVersion
Lua_IsXmlValid
cpp_EndSlaveConnect
cpp_MasterConnectSlave
cpp_StartSlaveConnect
```

These are not assumed missing. Plausible explanations include:

- registration through an alias whose C++ implementation has another name;
- a non-exported/static implementation;
- a macro or wrapper around another native function;
- a Lua global injected by native initialization;
- code paths retained in scripts but not present in this product build.

They should be resolved by tracing Lua registration tables and native initialization rather than guessed from names alone.

## Why ordinary Lua methods are filtered

A naive scan finds hundreds of calls such as `g_Manager:Method()` and Lua-owned class methods. Those are not automatically native APIs. The mapper therefore:

1. removes Lua functions defined within the analyzed script set;
2. removes ordinary Lua built-ins;
3. discards member calls on receivers assigned by Lua unless they look like native engine classes;
4. accepts arbitrary global names only with strong dynamic-symbol evidence;
5. accepts string-only evidence only when the name already follows a native-binding naming pattern.

This reduces the report to a reconstruction-oriented surface rather than a generic call graph.

## Usage

Run directly against a legally obtained original APK:

```bash
python3 tools/lua_native_api_map.py /path/to/com.hippiegame.nevergone.apk \
  --json build/lua-native-api.json \
  --markdown build/lua-native-api.md
```

The tool automatically reads `lib/armeabi-v7a/libcocos2dcpp.so` from the APK. It requires `nm`, `c++filt`, and `strings` for native evidence.

A decoded Lua directory can also be analyzed. In that case provide the original ELF separately:

```bash
python3 tools/lua_native_api_map.py build/decoded-assets/assets/Script \
  --elf /path/to/libcocos2dcpp.so \
  --markdown build/lua-native-api.md
```

Pass `--include-unreachable` to include the six Lua modules outside the normal `Game.StartLua` static graph.

## Reconstruction order derived from this map

1. Implement the seven `Game.StartLua` native-facing calls in the clean Android runtime.
2. Add a Lua 5.2-compatible runtime and execute the locally imported `Game.StartLua` without the original `libcocos2dcpp.so`.
3. Recreate the `ProtoRPC` surface required by `Game.Engine.RpcBase`.
4. Add filesystem helpers (`Lua_GetPathWithFileName`, `Lua_GetSetFilePath`, `Lua_CopyFile`, file-existence/XML helpers).
5. Reconstruct login/config bindings needed to reach the initial UI.
6. Add gameplay subsystem bindings only when the reachable execution path requests them.
7. Resolve the nine unconfirmed names through registration-table/xref analysis.

This order keeps the first boot milestone small while preserving a path to the full client API surface.
