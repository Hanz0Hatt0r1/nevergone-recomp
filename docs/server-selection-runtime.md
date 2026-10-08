# Server selection runtime contract

This note records the clean-room behavioral contract recovered for the original `NewServerList` path and the corresponding project-owned runtime state.

## Recovered native flow

The original ARM32 `libcocos2dcpp.so` retains dynamic symbols for the relevant login classes. The recovered flow is:

1. `cpp_OnGetServerList(lua_State*)` receives the JSON server list as argument 1 and `LastLoginServer` as argument 2.
2. `JsonDataManage::GetServerList()` constructs `ServerData` records.
3. `ServerInteractionLayer::GetServerList()` presents `NewServerList`.
4. `NewServerList::Tuijian(lastLoginServer)` preselects a row only when a server record has an exactly matching `id`.
5. `NewServerList::ccTouchEnded(...)` treats the gesture as a row tap only when absolute vertical movement is at most 10 pixels.
6. `NewServerList::GetTouchIDRect(...)` resolves a row sprite by rectangle containment. Its row tags are 1-based; no hit returns zero.
7. `NewServerList::UpDataServerSelet(int)` updates the selected server.
8. Confirm button tag `10002` reads the selected record and invokes `LUA_LOGIN::lua_CallGameRPC(ip, id)`.
9. That RPC wrapper resolves the Lua destination `g_UILogin.EnterGameLogicServer` with signature `s,i`.

Relevant recovered function addresses in the original ARM32 binary:

| Function | Address |
| --- | ---: |
| `LUA_LOGIN::lua_CallGameRPC(CCString,int)` | `0x002e8a34` |
| `JsonDataManage::GetServerList()` | `0x002f3eec` |
| `NewServerList::GetTouchIDRect(CCPoint)` | `0x002f8dd4` |
| `NewServerList::UpDataServerSelet(int)` | `0x002f8e24` |
| `NewServerList::ccTouchEnded(...)` | `0x002f8f68` |
| `NewServerList::Tuijian(int)` | `0x002f9178` |
| `NewServerList::init(ServerList)` | `0x002f94e8` |
| `NewServerList::buttonCallback(CCObject*)` | `0x002fa32e` |
| `ServerInteractionLayer::GetServerList()` | `0x002fadcc` |
| `cpp_OnGetServerList(lua_State*)` | `0x002faedc` |

## `ServerData` fields used by selection

The recovered record stride is `0x38` bytes. The fields consumed by this path are:

| Offset | Meaning |
| --- | --- |
| `+0x00` | integer `id` |
| `+0x04` | `CCString name` |
| `+0x1c` | `CCString ip` |
| `+0x34` | status-like integer initialized to `1` during server-list decoding |

The structured callback parser also preserves optional `BattleIP` because it is part of the recovered Lua/server JSON contract, but `NewServerList` does not use `BattleIP` when confirming a server. The confirmed call uses only `ip` and `id`.

## Project-owned state

`server_selection_state.{h,cpp}` models only the proven behavior:

- every new valid/invalid server payload replaces the previous selection domain;
- `LastLoginServer` preselects only an exact numeric `id` match;
- no match leaves selection unset rather than silently selecting the first row;
- a row selection is accepted only after a touch with vertical movement `<= 10.0` pixels;
- confirmation produces an `EnterRequest { ip, server_id, server_name }` corresponding to `g_UILogin.EnterGameLogicServer(ip, id)`;
- pending enter requests are consumable once and are invalidated by a newer server-list payload.

The callback bridge synchronizes this state whenever `cpp_OnGetServerList` is captured. The current runtime intentionally does **not** execute the resulting request yet: startup Lua execution still uses a temporary Lua state, so pretending to dispatch into a persistent `g_UILogin` table would be incorrect. Persistent/incremental Lua execution is a separate reconstruction step.

## Visual-resource boundary

The original binary references server-list resources such as:

- `gamescene_ui/ServerList/XMLFile1.xml`
- `gamescene_ui/ServerList/border1.png`
- `gamescene_ui/ServerList/border2.png`
- `ServerList/RANDOM.png`
- `ServerList/RANDOMName.png`

Those files are not present in the baseline APK archive used by this project. They may belong to downloaded/update/expansion content. The recompilation therefore does not present project-created graphics as original server-list assets. A later compositor may use user-imported copies if those resources are available, with an explicitly project-owned fallback otherwise.
