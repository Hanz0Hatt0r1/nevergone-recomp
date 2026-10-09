# Login callback payload reconstruction

This note records the clean-room data contract between the recovered Lua login flow and the reconstructed `ManagementLayer` UI state.

## Lua call shapes

The recovered `Game/UI/UILogin.lua` passes JSON strings across the Lua/native boundary rather than raw Lua tables:

```text
cpp_OnGetServerList(cjson.encode(tData.Serverlist), tData.LastLoginServer)
cpp_OnGetRoleList(cjson.encode(tCharacterList))
cpp_OnCreateTheRole(cjson.encode(tBaseInfo))
cpp_OnEnterGame(cjson.encode(strSaveData.GameUserBaseInfo))
```

Announcement delivery remains positional:

```text
cpp_OnGameAnnoucement(0, 0, title, content)
```

The callback bridge therefore preserves the original callback arguments and now also decodes the server/role JSON into project-owned UI models.

## Server-list schema

Static native evidence from `JsonDataManage::GetServerList()` names these fields:

- `id`
- `name`
- `ip`

The Lua login flow also reads `BattleIP` from each server object before entering the game-server path. The reconstructed model therefore retains all four when present:

```text
ServerEntry
  id
  name
  ip
  battle_ip
```

`LastLoginServer` is retained separately from the JSON array because the original Lua call passes it as the second callback argument.

Unknown JSON fields are ignored. Malformed JSON is rejected rather than partially promoted into UI state.

## Role-list schema

Static native evidence from `JsonDataManage::GetRoleList()` includes:

- `CharacterDataMap`
- `CharacterID`
- `CharacterName`
- `Career`
- `CharacterLevel`
- `CharacterCreateTime`
- `CharacterEx`
- `ClothesID`
- `ClothesColorID`
- `OnlineTimeCount`
- `GameUSETime`

The first reconstructed UI-facing model currently retains the fields needed for character-selection identity and appearance:

```text
RoleEntry
  character_id
  character_name
  career
  character_level
  clothes_id
  clothes_color_id
```

The parser walks nested arrays/objects and de-duplicates entries by `CharacterID`, which avoids depending on the exact container representation of `CharacterDataMap` while preserving the proven field contract.

## Online role-selection semantics

The project-owned `role_selection_state` consumes a valid parsed role list without introducing presentation assumptions. The recovered online ChooseHero path uses `Career` as the selectable item tag. Starting a selected item scans the role records and resolves the first record carrying that career, then uses that record's `CharacterID`.

The reconstructed state therefore keeps selection and dispatch separate:

```text
cpp_OnGetRoleList(JSON)
  -> parse RoleListPayload
  -> role_selection_state::sync_role_list(...)
  -> select career/index
  -> confirm_selection()
  -> pending CharacterID request
  -> g_UILogin.EnterGameWithCid(CharacterID)
```

The recovered create-role call shape is also retained as a pending request before Lua dispatch:

```text
request_create_role(name, career)
  -> g_UILogin.CreateCharacter(name, career)
```

Pending enter/create requests are consumed only after the matching Lua call succeeds. A changed selection or a fresh role-list callback invalidates stale pending requests.

## ManagementLayer routing

The parsed models live beside the raw callback payloads in `ClientUiSnapshot`. Projection still follows the callback-driven `ManagementRoute` state:

- server data is exposed only during `server-selection`;
- role data is exposed only during `role-selection`;
- moving to a later route hides the earlier structured model from the renderer;
- generation reset clears both raw and structured state.

This keeps data arrival separate from scene eligibility and prevents stale server/role data from leaking into a later reconstructed screen.

## Original NewServerList evidence

The original `NewServerList::init(ServerList)` references the following UI resources:

```text
gamescene_ui/ServerList/XMLFile1.xml
gamescene_ui/ServerList/border1.png
gamescene_ui/ServerList/border2.png
gamescene_ui/NEW_HONOR_UI/honor_title_bar.png
```

It also contains the string keys/labels `recommendedserver`, `allserverlist`, `xuanqu`, `qu`, `zhenchang`, `baoman`, and `xinqu`.

Those `gamescene_ui/ServerList/*` resource names are not present in the baseline APK ZIP inventory used for the current clean-room import tests. The renderer should therefore treat them as optional external/update-era assets rather than silently assuming the base APK importer failed.

## Verification

`tools/login_callback_payload_smoke.cpp` validates:

- server-array decoding and last-login-server retention;
- ignored unknown nested fields;
- malformed JSON rejection;
- nested role extraction and `CharacterID` de-duplication;
- empty-but-valid role lists.

`tools/role_selection_state_smoke.cpp` validates career-tag selection, first-match resolution for duplicate careers, pending enter/create request invalidation and one-shot consumption.

`tools/login_lua_dispatch_smoke.cpp` validates the recovered argument shapes for `EnterGameLogicServer`, `EnterGameWithCid` and `CreateCharacter`, including missing-function and Lua-error handling.

The parser/state/dispatch code is project-owned C++17 and does not depend on the original native library or proprietary decoded files.
