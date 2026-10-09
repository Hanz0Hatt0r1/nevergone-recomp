# ManagementLayer online role-selection view

This note records the first visible consumer for the structured `cpp_OnGetRoleList` state. It deliberately reuses only presentation details that are already supported by clean-room ChooseHero evidence and keeps the standalone save flow isolated.

## Route and data ownership

The compositor is active only while `initial_ui_transition::ManagementRoute` is `role-selection`. Its item list comes from the route-projected `ClientUiSnapshot::role_list_model`, while selection is owned by the separate online `role_selection_state` introduced for the recovered login flow.

It does **not** read or mutate:

- `offline_startup_flow`;
- `choose_hero_role_selection_state`;
- `DMG_01.sData` / `DMG_02.sData`;
- standalone profile metadata.

This prevents the online service path from changing the already reconstructed offline ChooseHero behavior.

## Reused recovered presentation

The shipped `ChooseHeroItem` layout contract already proves the role-board stack:

```text
centerX = 100
firstCenterY = visibleHeight - 100
nextCenterY = previousCenterY - (itemHeight + 15)
```

The online compositor therefore uses the same 1136x640 design-space mapping and the same user-imported `Login/ChooseHero` resources already staged by `ChooseHeroRoleAssetLoader`:

- `hero_board_a.png`
- `hero_board_b.png`
- `Hero_01_a.png`
- `Hero_01_b.png`
- `Hero_02_a.png`
- `Hero_02_b.png`

A selected role uses the B board/icon state; an unselected role uses A. `Career` 1 and 2 select the matching recovered hero artwork. Unknown careers still retain a selectable board but do not fabricate an icon.

The existing icon offset `(-30, 0)` relative to the role board is retained.

## Touch behavior

Android surface coordinates are mapped through the same aspect-fit 1136x640 transform used by the standalone role pane. A pointer must begin and end on the same board before the corresponding role-list index is passed to `role_selection_state::select_index()`.

The management compositor gets first refusal inside the generic native touch bridge only while its route is active. This means role-selection taps do not accidentally fall through into `TapToStart`, while all other routes preserve the existing touch path.

## Rendering boundary

This increment intentionally renders only the recovered role boards and career artwork above the current ManagementLayer fallback phase. It does not yet invent online profile labels, a background scene, or Play/Create controls.

The next online role increment should add the dynamic `CharacterName` / `CharacterLevel` profile presentation and then wire the proven Play action to `role_selection_state::confirm_selection()` plus `login_lua_session::dispatch_pending_role_enter_request()`.

## Verification

`tools/management_role_selection_view_smoke.cpp` checks the recovered role-stack geometry and board hit testing, including boundary behavior and invalid dimensions. The normal Android build remains responsible for compiling/linking the GLES compositor against the current NDK configuration.
