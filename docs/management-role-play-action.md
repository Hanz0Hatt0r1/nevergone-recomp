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
  -> EnterRoleRequest(CharacterID, Career, CharacterName, payload_generation)
  -> login_lua_session::ensure_started()
  -> login_lua_session::dispatch_pending_role_enter_request()
  -> g_UILogin.EnterGameWithCid(CharacterID)
  -> committed generation-safe role handoff
  -> awaiting-enter-game
  -> cpp_OnEnterGame
  -> entering-game / GameLevels enter transition
```

The login session consumes the pending request only after the Lua call succeeds. If Lua startup or dispatch fails, the request remains pending for diagnostics/retry.

After a successful call the exact dispatched `CharacterID`, `Career`, `CharacterName`, and source role-list generation are retained by `role_selection_state`. A newer role-list payload invalidates that handoff, and a single generation cannot commit a second successful role enter. This makes the Play boundary generation-safe and one-shot even though the request itself has already been consumed.

`EnterGameWithCid` is allowed to invoke `cpp_OnEnterGame` synchronously. If that callback arrives before the Lua call returns, the route is already `entering-game` and the post-dispatch commit is still treated as successful. Otherwise the project exposes `awaiting-enter-game` until the recovered callback arrives; it does not synthesize the callback.

The `cpp_OnCreateTheRole` auto-enter path uses the same committed handoff and `awaiting-enter-game` boundary after its inline `EnterGameWithCid` call succeeds.

## Separation from standalone ChooseHero

The online action does not read or mutate:

- `offline_startup_flow`
- `choose_hero_action_state`
- `choose_hero_role_selection_state`
- `DMG_01.sData` / `DMG_02.sData`

The only shared pieces are recovered presentation assets/layout and the 1136x640 surface mapping.

## First boundary after enter-game

The real `cpp_OnEnterGame` callback remains the owner of the gameplay handoff. `client_callback_bridge` immediately calls `game_levels_enter_transition::on_enter_game(files_dir)`, whose first concrete asset gate is the user-imported `assets/gamescene/gs_list/pvp_scene.glData`. The probe must validate the currently recovered GameLevels sections through the port-node section before the retained runtime model and render queue are built.

This means the role/ChooseHero path no longer requires the original native library to reach the project-owned enter-game boundary. The first post-enter data dependency is the imported and validated GameLevels scene stream; unresolved GameLevels fields are not guessed here.

## Verification boundary

Host smoke coverage validates:

- role selection and `CharacterID` request creation in `role_selection_state_smoke.cpp`;
- generation-tagged committed handoff, duplicate rejection, and stale-generation rejection in `role_selection_state_smoke.cpp`;
- `role-selection -> awaiting-enter-game -> entering-game`, including created-role auto-enter routing, in `initial_ui_transition_smoke.cpp`;
- the exact `g_UILogin.EnterGameWithCid(CharacterID)` call/error shape in `login_lua_dispatch_smoke.cpp`;
- type-1 button layout/hit boxes in the ChooseHero action-control layout smoke.

The Android CI build provides the integration check for the GLES/input compositor, JNI router linkage, Lua runtime linkage, and 16 KiB page compatibility.
