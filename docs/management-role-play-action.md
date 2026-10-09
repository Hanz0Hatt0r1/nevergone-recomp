# ManagementLayer online role Play action

This increment connects the visible online role selection to the recovered login execution path without reusing standalone save semantics.

## Proven presentation contract

The online Play control reuses the reconstructed type-1 `MRFixedButton` presentation already used by `ChooseHero::HeroInformation`:

- normal: `Common/btn_standard_a.png`
- pressed: `Common/btn_standard_b.png`
- disabled store entry: `Common/btn_standard_c.png`
- localized Play label rendered at 24 px Arial
- right edge at 90% of the 1136 design width
- bottom placement derived from the button height plus the recovered 5 px gap
- label maximum width 115 px, shrink-only

`ChooseHeroActionControlLoader` already stages these optional update-era resources into the project-owned native asset store during runtime configuration. No additional proprietary resource is added by this increment.

## Route and visibility

`management_role_action_control_compositor` is active only while `ManagementRoute::kRoleSelection` is active and only after `role_selection_state` contains a selected online role.

It renders only the Play control. The standalone Delete action remains owned by `choose_hero_action_control_compositor` and `choose_hero_action_state`; online role state never enters that path.

## Input and execution

The existing Java touch path enters `nativeOnServerSelectionTouch` before the generic renderer touch bridge. The online Play control is given first refusal there before role-board selection.

A Play click requires DOWN and UP on the same recovered hit box. On release:

```text
role_selection_state::confirm_selection()
  -> EnterRoleRequest(CharacterID, Career, CharacterName)
  -> login_lua_session::ensure_started()
  -> login_lua_session::dispatch_pending_role_enter_request()
  -> g_UILogin.EnterGameWithCid(CharacterID)
```

The login session consumes the pending request only after the Lua call succeeds. If Lua startup or dispatch fails, the request remains pending for diagnostics/retry.

## Separation from standalone ChooseHero

The online action does not read or mutate:

- `offline_startup_flow`
- `choose_hero_action_state`
- `choose_hero_role_selection_state`
- `DMG_01.sData` / `DMG_02.sData`

The only shared pieces are recovered presentation assets/layout and the 1136x640 surface mapping.

## Verification boundary

The existing host smoke coverage already validates:

- role selection and `CharacterID` request creation in `role_selection_state_smoke.cpp`;
- the exact `g_UILogin.EnterGameWithCid(CharacterID)` call/error shape in `login_lua_dispatch_smoke.cpp`;
- type-1 button layout/hit boxes in the ChooseHero action-control layout smoke.

The Android CI build provides the integration check for the new GLES/input compositor, JNI router linkage, Lua runtime linkage, and 16 KiB page compatibility.
