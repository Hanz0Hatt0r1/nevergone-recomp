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
| `NewServerList::GetDrawRectSp(CCSprite*)` | `0x002f8ce8` |
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

## Recovered row layout and hit rectangles

`NewServerList::init` reveals the row placement independently of the missing artwork. The row sprite uses anchor `(0.0, 0.5)` and tags rows from `1` in creation order. Rows are arranged in two columns:

- first-column left X: `56`;
- second-column left X: `rowWidth + 96`, which leaves a 40-pixel horizontal gap after a first-column row of width `rowWidth`;
- first row-pair center Y: `320`;
- each subsequent row pair moves down by `90` pixels (`320`, `230`, `140`, `50`, ...).

For zero-based project index `i` this becomes:

```text
column  = i % 2
row     = i / 2
leftX   = column == 0 ? 56 : rowWidth + 96
centerY = 320 - 90 * row + scrollOffsetY
bottomY = centerY - rowHeight / 2
```

`GetDrawRectSp` confirms that normal server-row hit rectangles use the sprite's full content width/height and add the scrolling content layer's Y position before `containsPoint`. This is represented by `server_selection_layout.{h,cpp}`. The row width and height are deliberately caller-supplied because the original `border1.png` is absent from the baseline APK; no guessed dimensions are embedded in the runtime.

The broader reconstructed UI uses the verified 1136x640 design-canvas contract. `server_selection_layout` records that contract but keeps surface-to-design conversion out of the row module so a later compositor can share the same viewport policy as the rest of the reconstructed UI.

## Visual-resource boundary

The original binary references server-list resources such as:

- `gamescene_ui/ServerList/XMLFile1.xml`
- `gamescene_ui/ServerList/border1.png`
- `gamescene_ui/ServerList/border2.png`
- `ServerList/RANDOM.png`
- `ServerList/RANDOMName.png`

Those files are not present in the baseline APK archive used by this project. A direct filename search on the connected project Drive also did not expose standalone copies. They may belong to downloaded/update/expansion content. The recompilation therefore does not present project-created graphics as original server-list assets. A later compositor may use user-imported copies if those resources are available, with an explicitly project-owned fallback otherwise.
