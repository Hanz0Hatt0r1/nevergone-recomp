# Server selection runtime contract

This note records the clean-room behavioral contract recovered for the original `NewServerList` path and the corresponding project-owned runtime state.

## Recovered native flow

The original ARM32 `libcocos2dcpp.so` retains dynamic symbols for the relevant login classes. The recovered flow is:

1. `cpp_OnGetServerList(lua_State*)` receives the JSON server list as argument 1 and `LastLoginServer` as argument 2.
2. `JsonDataManage::GetServerList()` constructs `ServerData` records.
3. `ServerInteractionLayer::GetServerList()` presents `NewServerList`.
4. `NewServerList::Tuijian(lastLoginServer)` preselects a row only when a server record has an exactly matching `id`.
5. The main view contains menu item tag `10001` built from `gamescene_ui/ServerList/border1.png` and centered at `(visibleWidth/2, 200)`.
6. `NewServerList::buttonCallback()` handles tag `10001` by revealing the hidden server-list `CCLayerColor`, raising it to z-order `5`, enabling `NewServerList` touch handling and disabling the main confirm menu.
7. `ccTouchBegan/Moved/Ended` delegate motion to `VEScrollView`; `ccTouchEnded` treats the gesture as a row tap only when absolute vertical movement is at most `10` pixels.
8. `GetTouchIDRect(...)` resolves a row sprite by rectangle containment. Row tags are 1-based; no hit returns zero.
9. `UpDataServerSelet(int)` updates the selected server, hides the server-list layer again, restores the main confirm menu and disables list touch handling.
10. Confirm button tag `10002` reads the selected record and invokes `LUA_LOGIN::lua_CallGameRPC(ip, id)`.
11. That RPC wrapper resolves `g_UILogin.EnterGameLogicServer` with signature `s,i`.

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
- the chooser starts closed;
- reconstructed tag `10001` opens it idempotently;
- row touches are accepted only while the chooser is open and vertical movement is `<=10.0` design pixels;
- a valid row selection closes the chooser, matching `UpDataServerSelet()`;
- confirmation is blocked while the chooser is open;
- confirmation produces `EnterRequest { ip, server_id, server_name }` corresponding to `g_UILogin.EnterGameLogicServer(ip, id)`;
- pending enter requests are invalidated by a newer server-list payload.

The callback bridge synchronizes this state whenever `cpp_OnGetServerList` is captured. `login_lua_session` keeps the reconstructed `Game.StartLua` state alive, and the server-selection touch bridge attempts `g_UILogin.EnterGameLogicServer(ip,id)` after confirm. The request is consumed only after successful Lua dispatch. Missing tables/functions, startup failures and Lua exceptions leave it pending and are reported in diagnostics.

This does not imply that retired online services are available: `ProtoRPC` remains a clean-room boot-safe service boundary unless an offline-compatible replacement becomes necessary for the preservation path.

## Recovered selector control (`border1`)

The user-supplied original expansion tree contains:

```text
gamescene_ui/ServerList/border1.png
```

Focused Thumb disassembly of `NewServerList::init` shows that `border1.png` is passed as **both** the normal and selected filenames to `CCMenuItemImage::create`. The item is assigned tag `10001` and positioned at:

```text
x = visibleWidth / 2
y = 200
```

On the verified `1136x640` design canvas this is center `(568,200)`. The supplied decoded image is `405x46`, so its recovered hit rectangle is centered on that point and uses the runtime bitmap dimensions.

The same block creates selector-local labels from `XMLFile1.xml`:

- key `qu` at local X `95`, vertical center of the `border1` item;
- key `xuanqu` at local X `340`, vertical center of the item.

The loader stages these shipped strings as rendered label assets. Dynamic server-id/name labels are also present in the original control (`UpDataServerSelet` updates the objects stored at `+0x1b8` and `+0x1bc`), but their final reconstructed text layout is kept as a separate boundary rather than guessed.

`buttonCallback(tag=10001)` proves the two-stage interaction: `border1` is the main selector that opens the hidden full server list; it is not a server row and is not merely decorative.

## Recovered row layout and hit rectangles (`border2`)

The per-server row sprite resolves to:

```text
gamescene_ui/ServerList/border2.png
```

The row sprite uses anchor `(0.0, 0.5)` and tags rows from `1` in creation order. Rows are arranged in two columns:

- first-column left X: `56`;
- second-column left X: `rowWidth + 96`;
- first row-pair center Y: `320`;
- subsequent row pairs move down by `90` pixels (`320`, `230`, `140`, `50`, ...).

For zero-based project index `i`:

```text
column  = i % 2
row     = i / 2
leftX   = column == 0 ? 56 : rowWidth + 96
centerY = 320 - 90 * row + scrollOffsetY
bottomY = centerY - rowHeight / 2
```

The supplied decoded `border2.png` is `499x68`, therefore the second-column left X is `595`. The runtime does not hard-code these dimensions for rendering: `ServerSelectionAssetLoader` reads the user-imported image and exposes its actual size to both drawing and hit testing. `499x68` is retained in tests as evidence for the supplied expansion version.

`GetDrawRectSp` adds the scrolling content layer's Y position to normal row rectangles before containment checks. The current layout API already accepts this offset.

## Recovered confirm control

`NewServerList::init` creates the enter-game control through `createMRFixedButton` with type `1`, assigns tag `10002`, and places it at:

```text
x = visibleWidth / 2
y = 100
```

For the `1136x640` design surface the center is `(568,100)`. The expansion tree contains the same type-1 standard-button family already recovered for ChooseHero:

```text
Common/btn_standard_a.png  normal
Common/btn_standard_b.png  pressed
Common/btn_standard_c.png  disabled
```

All three supplied states decode to `162x63`. The same runtime dimensions drive rendering and touch hit testing.

`gamescene_ui/ServerList/XMLFile1.xml` supplies the `start` label used by the original control. `ServerSelectionAssetLoader` renders it rather than embedding translated text in project code. The loader is independent of the ChooseHero route even though both controls share the original standard-button artwork family.

If expansion assets are unavailable, project-owned fallback controls remain at the recovered centers rather than the previous temporary positions.

## Expansion encoding/import boundary

Raw expansion `.png`, `.csv`, `.lua`, and `.hpc` files use the same byte encoding already implemented by `OriginalObbImporter`. For each file the importer starts a counter at zero and applies:

```text
decoded = ((encoded XOR 1) - counter) mod 256
counter = (counter + 1) mod 127
```

The importer writes decoded results under the app-private `files/assets` tree. Runtime loaders therefore use ordinary `BitmapFactory.decodeFile()` on those imported files; no proprietary resource bytes are checked into this repository.

The same expansion tree contains `serverlist.csv`; after the existing transform it becomes ordinary UTF-8 CSV. This confirms the encoding path but does not replace the callback-provided server model used by `server_selection_state`.

## Surface mapping and visible rendering

`server_selection_view.{h,cpp}` maps Android top-left surface coordinates into the verified `1136x640` design canvas using aspect-fit letterboxing. The same mapping is used for drawing and input.

`server_selection_compositor.{h,cpp}` is wired into the normal `GameSurfaceView` GL lifecycle and is active only while `ManagementRoute` is `server-selection` with a valid non-empty server payload.

The visible states now mirror the recovered modal split:

```text
main-selector
  border1 / tag 10001 at (568,200)
  confirm / tag 10002 at (568,100) when a server is selected

chooser-overlay
  hidden CCLayerColor semantic equivalent
  border2 server rows
  main confirm path disabled
```

A successful row choice returns from `chooser-overlay` to `main-selector`. Imported GLES textures are generation-aware and rebuilt after asset reload or EGL-context recreation.

The dark chooser panel and selected-row tint remain explicitly project-owned fallback visuals; exact original artwork/effect for those pieces has not been claimed.

## Scrolling boundary

The original `ccTouchBegan`, `ccTouchMoved`, and `ccTouchEnded` forward node-space touch coordinates to `VEScrollView::TouchesBegan/Moved/Ended`. After `TouchesEnded`, `NewServerList` independently compares the current and begin Y positions and accepts a row tap only when absolute movement is at most `10` pixels.

The exact internal `VEScrollView` inertia/clamp model has not yet been reconstructed, so the current compositor does not invent scrolling physics. `server_selection_layout` already accepts `scrollOffsetY`, allowing the recovered scroll model to be connected later without changing row geometry.

## Verification

`tools/server_selection_state_smoke.cpp` covers the recovered modal lifecycle: chooser closed after payload sync, idempotent tag-10001 open, row selection only while open, automatic close after valid selection, and blocked confirm while open.

`tools/server_selection_layout_smoke.cpp` retains generic geometry fixtures and pins the supplied expansion row evidence (`499x68`, second-column X `595`) without committing original image bytes.

`tools/server_selection_view_smoke.cpp` pins selector tag `10001` at `(568,200)` with supplied `405x46` geometry, confirm tag `10002` at `(568,100)` with supplied `162x63` geometry, and aspect-fit surface mapping.
