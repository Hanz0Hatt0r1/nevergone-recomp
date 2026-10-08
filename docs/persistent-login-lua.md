# Persistent reconstructed login Lua runtime

The modern runtime previously executed `Game.StartLua` only inside a diagnostic helper that created and destroyed a fresh `lua_State`. That was sufficient for missing-binding discovery, but it could not support interactive login actions: a confirmed server selection had no surviving `g_UILogin` table to call.

This layer introduces a long-lived, project-owned Lua session without changing the clean-room service boundary.

## Lifecycle

`login_lua_session` owns one Lua 5.2.3 state at a time:

1. runtime configuration resets any previous session;
2. the first bootstrap diagnostic starts or retries the session;
3. the session opens Lua standard libraries;
4. all reconstructed native bindings are registered;
5. the missing-global probe is installed;
6. `Game.StartLua` is executed through the existing `CAddDoString`/module loader;
7. on success, the same `lua_State` remains alive for interactive login calls;
8. a later runtime reconfiguration closes the state before replacing paths/device/version data.

A start failure is not permanent. Importing the user-owned APK/OBB and refreshing diagnostics can retry `Game.StartLua` using the newly available decoded resources.

Runtime diagnostics no longer call the old temporary `lua_runtime::smoke_test()`/`startup_execution_report()` path. This matters because registering callbacks in a temporary state reset global reconstructed callback state. CI still runs the Lua compatibility smoke independently.

## Reconstructed server dispatch

The recovered native call chain established the semantic destination:

```text
NewServerList confirm (tag 10002)
  -> LUA_LOGIN::lua_CallGameRPC(ip, id)
  -> g_UILogin.EnterGameLogicServer(ip, id)
```

`login_lua_dispatch` performs exactly that table-field call with two Lua arguments (`string`, `integer`) and no implicit `self` argument.

`server_selection_state` now exposes a non-destructive `peek_pending_enter_request()`. The persistent session:

- reads the pending request;
- calls `g_UILogin.EnterGameLogicServer(ip,id)`;
- consumes the pending request only after successful Lua completion;
- preserves the request and records an error if the table/function is unavailable or the Lua call throws.

`GameSurfaceView` server-selection touch JNI attempts this dispatch after a confirm release. If the session has not started successfully yet, it retries startup first.

## What this does not recreate

Keeping the Lua state alive does not make obsolete online services available. `ProtoRPC` remains a boot-safe reconstructed shell unless/until an offline-compatible replacement is justified by the reachable preservation path. A successful Lua dispatch means the original script-side login logic can run in the reconstructed state; network-dependent operations may still intentionally stop at service boundaries.

Role selection, scene entry and offline gameplay should now build on this persistent state rather than creating additional temporary Lua VMs.

## Regression coverage

`tools/login_lua_dispatch_smoke.cpp` links against the verified Lua 5.2.3 source in CI and verifies:

- `g_UILogin.EnterGameLogicServer` receives exactly the IP string and server id;
- no implicit table/self argument is injected;
- missing table and missing function failures are explicit;
- Lua exceptions are surfaced without corrupting the caller's stack contract.

The existing server-selection state regression also verifies that peeking a pending request does not consume it.
