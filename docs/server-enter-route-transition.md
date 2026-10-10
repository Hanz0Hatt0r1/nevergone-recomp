# Server enter route transition

This note records the project-owned transition after the recovered server-selection confirm path successfully dispatches `g_UILogin.EnterGameLogicServer(ip, id)`.

## Runtime contract

The reconstructed path is now:

```text
server-selection
  -> confirm selected server
  -> pending EnterRequest { ip, server_id, server_name }
  -> successful persistent-Lua EnterGameLogicServer dispatch
  -> awaiting-role-list
  -> cpp_OnGetRoleList
  -> role-selection
```

`awaiting-role-list` is an explicit project-owned semantic route. It makes successful server-entry dispatch observable without fabricating a callback that belongs to the original login/network flow.

The transition is deliberately conditional:

- a failed Lua startup or failed `EnterGameLogicServer` call leaves the route at `server-selection`;
- failed dispatch leaves the pending request intact for diagnostics/retry;
- only a successful dispatch consumes the pending request and enters `awaiting-role-list`;
- repeated success notifications outside `server-selection` are idempotently rejected;
- only the real captured `cpp_OnGetRoleList` callback advances to `role-selection`.

This preserves the recovered callback ownership while preventing the server-selection compositor from remaining visible after an enter request was accepted locally.

## Host regression coverage

`tools/server_selection_state_smoke.cpp` verifies the server-selection side of the boundary, including exact `LastLoginServer` preselection, modal selection behavior, the selected `id/name/ip` request payload, and one-shot pending-request consumption.

`tools/initial_ui_transition_smoke.cpp` verifies the route side of the boundary: premature success notifications are rejected, `server-selection -> awaiting-role-list` occurs once after success, duplicate notifications do not increment the route transition count, and `cpp_OnGetRoleList` owns the final `awaiting-role-list -> role-selection` transition.

## Remaining gap to ChooseHero / role selection

The project can now represent the full local transition through a successful server-enter dispatch, but a complete live online path still depends on receiving a valid `cpp_OnGetRoleList` payload from the original service protocol or a future preservation/offline-compatible service replacement. The project does not synthesize that payload merely to advance the UI.

Once a valid role-list callback is captured, the existing callback parser and Management role-selection/ChooseHero reconstruction own the next visible route. This change does not add proprietary asset bytes and does not alter the service boundary.
