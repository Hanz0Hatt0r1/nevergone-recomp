# Runtime startup diagnostics

The clean runtime now records missing Lua globals without replacing them with fake implementations. This keeps failures honest while making the next reconstruction target explicit.

## Missing-global probe

A fresh Lua 5.2.3 state installs a metatable on `_G` with a diagnostic `__index` handler. When a script reads an undefined global, the handler records the name and returns `nil`, matching normal Lua semantics.

The startup report therefore includes entries such as:

```text
missing globals observed: 2
  - Lua_GetPathWithFileName (1)
  - ProtoRPC (3)
```

The traceback remains the primary failure signal. The missing-global list is supporting evidence that helps distinguish the first true blocker from unrelated globals touched earlier in the same startup path.

## Registry structure

`native_binding_registry.{h,cpp}` is the central registration point for reconstructed Lua-visible native APIs. At the current stage it registers the known `Game.StartLua` bindings and installs diagnostics. Future filesystem, RPC, UI, save-data, and gameplay bindings should be added through this registry rather than directly in the boot code.

## Smoke test

The Lua smoke test intentionally reads `NeverGoneMissingProbe`. The expected behavior is:

1. Lua receives `nil`;
2. the probe records exactly one hit for `NeverGoneMissingProbe`;
3. the smoke report prints `missing-global probe: ok`.

This validates the diagnostic path even when proprietary game scripts have not been imported.

## Reconstruction rule

Do not add generic stubs that silently return placeholder values for unknown native APIs. Implement a binding only when its behavior is sufficiently understood or when a deliberately narrow compatibility behavior is documented. Unknown globals should remain visible as failures so the recompilation does not drift away from the original client behavior.
