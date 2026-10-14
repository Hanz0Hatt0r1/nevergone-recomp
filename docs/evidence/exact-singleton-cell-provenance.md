# Exact singleton cell provenance probe

Evidence-only follow-up for the two slot-`0x8` unresolved receivers whose effective LOAD addresses fold to writable `.bss` singleton cells.

The independently captured owner audit identifies:

- `libcocos2dcpp.so:0029edb4` call `0` -> cell `0x0094cb44`, owned by `ActionDataManager::sharedActionDataManager` / `ActionDataManager::end`.
- `libcocos2dcpp.so:002a08fc` call `1` -> cell `0x0094cbd8`, owned by `AndroidSdkMgr::getInstance` / `AndroidSdkMgr::Destroy`.

This probe does **not** infer a receiver class from those names or from nearby constructor evidence. `ExportExactSingletonCellProvenance.java` decompiles only the selected accessor and accepts a high-P-code `STORE` or `LOAD` as cell evidence only when the corresponding instruction has a direct low-level reference to the exact singleton cell. The offline audit then requires the same value root to be passed as `this` to a class-qualified constructor and stored to the cell, plus an exact-cell LOAD root that reaches RETURN. Only then is the constructor class mapped through the static vtable catalog and the unresolved dispatch slot checked.

No inference/query/routing state is modified by this probe.
