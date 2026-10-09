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

## Project-owned state and Lua dispatch

`server_selection_state.{h,cpp}` models only the proven behavior:

- every new valid/invalid server payload replaces the previous selection domain;
- `LastLoginServer` preselects only an exact numeric `id` match;
- no match leaves selection unset rather than silently selecting the first row;
- a row selection is accepted only after a touch with vertical movement `<= 10.0` pixels;
- confirmation produces an `EnterRequest { ip, server_id, server_name }` corresponding to `g_UILogin.EnterGameLogicServer(ip, id)`;
- pending enter requests are invalidated by a newer server-list payload.

The callback bridge synchronizes this state whenever `cpp_OnGetServerList` is captured. `login_lua_session` keeps the reconstructed `Game.StartLua` state alive, and the server-selection touch bridge attempts `g_UILogin.EnterGameLogicServer(ip,id)` after confirm. The request is peeked non-destructively and consumed only after a successful Lua call. Missing tables/functions, startup failures and Lua exceptions leave it pending and are reported in runtime diagnostics.

This does not imply that retired online services are available: `ProtoRPC` remains a clean-room boot-safe service boundary unless an offline-compatible replacement becomes necessary for the preservation path.

## Recovered row layout and hit rectangles

`NewServerList::init` reveals the row placement. The row sprite uses anchor `(0.0, 0.5)` and tags rows from `1` in creation order. Rows are arranged in two columns:

- first-column left X: `56`;
- second-column left X: `rowWidth + 96`;
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

`GetDrawRectSp` confirms that normal server-row hit rectangles use the sprite's full content width/height and add the scrolling content layer's Y position before `containsPoint`.

### Expansion-resource confirmation

The user-supplied original expansion asset tree now provides the previously missing `gamescene_ui/ServerList` resources. Focused Thumb disassembly of shipped `NewServerList::init` resolves the per-server `CCSprite::create()` filename to:

```text
gamescene_ui/ServerList/border2.png
```

The imported/decoded `border2.png` content size is `499x68`. Therefore, with the original row artwork loaded:

```text
rowWidth  = 499
rowHeight = 68
second-column left X = 499 + 96 = 595
```

The runtime does not hard-code that bitmap size for rendering. `ServerSelectionAssetLoader` reads the user-imported decoded image, the native asset store exposes its runtime dimensions, and those dimensions are passed to both `server_selection_layout` and touch hit testing. The `499x68` values are retained in regression coverage as clean-room evidence for the supplied expansion version.

`border1.png` is also present and decodes to `405x46`, but its exact `NewServerList` role remains a separate recovery boundary; it is staged without being assigned speculative behavior.

## Expansion encoding/import boundary

Raw expansion `.png`, `.csv`, `.lua`, and `.hpc` files use the same byte encoding already implemented by `OriginalObbImporter`. For each file the importer starts a counter at zero and applies:

```text
decoded = ((encoded XOR 1) - counter) mod 256
counter = (counter + 1) mod 127
```

The importer writes the decoded result under the app-private `files/assets` tree. Runtime loaders therefore use ordinary `BitmapFactory.decodeFile()` on those imported files; no proprietary resource bytes are checked into this repository.

The same expansion tree contains `serverlist.csv`; after the existing import transform it is ordinary UTF-8 CSV. This confirms the encoding path but does not replace the callback-provided live/reconstructed server model used by `server_selection_state`.

## Surface mapping and visible rendering

`server_selection_view.{h,cpp}` maps Android top-left surface coordinates into the verified `1136x640` design canvas using aspect-fit letterboxing. The same mapping is used for drawing and input.

`server_selection_compositor.{h,cpp}` is wired into the normal `GameSurfaceView` GL lifecycle. It is active only while the reconstructed `ManagementLayer` route is `server-selection` and a valid server payload is present. It draws after the recovered SingleLogin/splash layers.

When expansion `border2.png` is available, rows use the original imported texture and its actual content dimensions. GLES texture state is generation-aware and is rebuilt after asset reload or EGL-context recreation. When the expansion row asset is absent, the project-owned `440x72` fallback remains available so the baseline-APK path does not regress.

The surrounding dark panel, selected-row overlay and confirm control are still explicitly project-owned fallback visuals in this increment. They are not claimed to reproduce the original artwork. A focused ARM pass has separately confirmed the original confirm control center at `(visibleWidth/2, 100)`, tag `10002`, and type-1 standard-button family; that should be integrated as a separate verified increment rather than mixed into the row-resource change.

`GameSurfaceView.onTouchEvent` offers each pointer event to the server-selection compositor first. If the server route is inactive, the compositor returns `false` and the existing `nativeOnTouch`/TapToStart path remains unchanged. While active, row taps use the recovered `<=10` design-pixel vertical movement rule before updating selection.

Scrolling is not yet reconstructed in the compositor. `server_selection_layout` already supports a content-layer Y offset, so a later verified scroll model can be connected without changing row placement or hit-test semantics.

## Verification

`tools/server_selection_layout_smoke.cpp` retains generic geometry fixtures and now additionally pins the supplied expansion row evidence (`499x68`, second-column X `595`) without committing any original image bytes.
