# Role creation name validation

This increment extracts the recovered role-name checks from the Lua-only binding layer so the eventual ManagementLayer create-role UI and the imported Lua scripts can share one clean-room implementation.

## Recovered submit order

`ChooseHero::OnCreateback(tag=4)` proves this native submit sequence:

1. reject an empty text-field string and show the original two-stage message path;
2. validate through `ManagementLayer::LGG_CheckStringLegal(...)`;
3. measure the validated UTF-8 buffer with `strlen`;
4. accept at most 21 bytes;
5. call `LUA_LOGIN::CreateTheRole(name, createRoleParameter)`.

`role_creation_validation::validate_role_name()` reproduces only the first four checks. It deliberately does **not** dispatch a role creation request yet.

## Shared keyword filter

The existing `Lua_CheckStringLegal` and `Lua_CheckNickName` implementations previously kept their helpers private inside `string_validation_bindings.cpp`. The reusable behavior now lives in `string_validation.{h,cpp}`:

- `string_is_legal()` loads the user-imported `assets/newWord.txt`, handles UTF-8 BOM / CRLF input and rejects names containing a blocked dictionary entry;
- `nickname_is_valid()` preserves the separate Lua nickname character contract (ASCII letters/digits or Han codepoints).

The Lua C functions delegate to these shared helpers, so extracting the code does not create a second validation implementation.

## Important boundary

The recovered native tag-4 path does **not** prove a `Lua_CheckNickName` call before `CreateTheRole`. The role submit validator therefore does not add that restriction. Doing so would make the recompilation stricter than the observed original path.

The current evidence also does not yet prove that the native `createRoleParameter` field is numerically identical to the `career` argument used by `g_UILogin.CreateCharacter(name, career)`. This increment intentionally leaves that mapping unresolved rather than silently equating the two integers.

## Verification

`tools/role_creation_validation_smoke.cpp` covers:

- empty names;
- imported dictionary matching including UTF-8 BOM and CRLF;
- the exact 21-byte boundary;
- UTF-8 byte counting rather than codepoint counting;
- preservation of the separate `Lua_CheckNickName` character rule;
- punctuation remaining acceptable to tag-4 validation when it is not blocked by the dictionary.

The fast PR CI runs this smoke test together with the existing login/server/role critical-path state tests. The full Android build then verifies the extracted validator and unchanged Lua wrappers link into the native library.
