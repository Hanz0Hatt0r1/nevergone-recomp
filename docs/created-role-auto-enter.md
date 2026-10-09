# Created-role auto-enter contract

This note records clean-room behavior recovered from the shipped ARMv7 `LUA_LOGIN::cpp_OnCreateTheRole(lua_State*)`, `LUA_LOGIN::CreateTheRoleSuccessful()`, `LUA_LOGIN::lua_SelectCharactersStartTheGame(int)`, and the imported login Lua `UILogin:OnServerCreateCharacter(...)` path. Original disassembly and proprietary Lua sources are not stored in the repository.

## Server callback payload

The shipped Lua create-character response decodes `GameUserBaseInfo`, checks that `CharacterID` exists, and on the normal release path calls:

```text
cpp_OnCreateTheRole(cjson.encode(tBaseInfo))
```

The recompilation reuses its bounded structured role parser for this JSON. Auto-enter is armed only when the callback resolves to one unique role record with a nonzero `CharacterID`; malformed/ambiguous callback data remains visible as raw callback diagnostics and is not guessed.

## Native success behavior

The shipped `cpp_OnCreateTheRole` parses the returned role into the ManagementLayer selected-role record and then calls `CreateTheRoleSuccessful()`.

`CreateTheRoleSuccessful()` performs two important side effects:

1. removes/clears the active CharacterName layer;
2. takes the returned role's `CharacterID` and calls `lua_SelectCharactersStartTheGame(CharacterID)`.

On the online branch, that start path reaches the same semantic operation used by normal role selection:

```text
g_UILogin.EnterGameWithCid(CharacterID)
```

The game therefore does **not** remain parked on a `role-created` screen after a successful fresh-account create response.

## Reconstructed transition

The clean-room runtime now mirrors this behavior as:

```text
cpp_OnCreateTheRole(json)
  -> parse one returned RoleEntry
  -> CharacterName state complete_creation()
  -> stage created_role_transition(RoleEntry)
  -> stage role_selection_state direct EnterRoleRequest
  -> g_UILogin.EnterGameWithCid(CharacterID)
```

The direct enter request does not depend on the previous `cpp_OnGetRoleList` payload. This matters for a fresh account whose pre-create role list was empty.

## Lua re-entry boundary

The original native implementation enters the newly created role directly from the create callback. The reconstructed callback follows that evidence but avoids recursively entering `login_lua_session`'s mutex:

- callback/UI state is captured under `client_callback_bridge`'s mutex;
- that mutex is released;
- CharacterName completion and created-role staging occur;
- `created_role_transition::pump_inline(lua_State*)` uses the same callback Lua state and the already verified `login_lua_dispatch::call_enter_game_with_cid(...)` helper directly.

Thus the nested Lua call is preserved without attempting to lock the persistent-session wrapper from inside its own callback stack.

## Failure semantics

`created_role_transition` is one-shot by default. A failed automatic `EnterGameWithCid` attempt leaves the created role pending but marks dispatch as not due, preventing a frame/tick loop from hammering the login call. `request_retry()` explicitly arms one additional attempt.

The normal success path consumes the matching `role_selection_state` enter request and records the transition as dispatched. A request whose ID/career/name changes during dispatch is treated as a failure rather than silently consuming unrelated state.

## CharacterName completion vs Cancel

Server success is not modeled as the user pressing tag `2`/Cancel. `character_name_state::complete_creation()` closes the active layer and clears pending randomization while incrementing a separate completion counter. It does not increment `action_count` or `close_count`.

This keeps diagnostics aligned with the shipped distinction between a user callback and `CreateTheRoleSuccessful()` cleanup.
