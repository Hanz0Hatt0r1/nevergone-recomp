# Bounded P-code exporter output parity

**Run date:** 2026-10-17  
**Ghidra:** `12.1.2_DEV`  
**Project:** `nevergone`  
**Program:** `libcocos2dcpp.so`  
**Mode:** `-readOnly -noanalysis`

Both implementations processed the same analyzed program and root function,
`EnemyActionsData::loadWBGFile` at `0029f430`. The shared scripts were taken
from `revenginstr` main at `a9bac53d`; the Nevergone fallbacks match the
current consumer main sources at `80a67f73`.

| Export | Depth | Function limit | Functions | Rows | Cap reached | Record parity |
|---|---:|---:|---:|---:|---|---|
| Bounded forward callgraph | 2 | 24 | 24 | 8,475 | yes | byte-identical |
| Bounded reverse callgraph | 2 | 24 | 2 | 8,171 | no | byte-identical |

The machine reports are [forward](ghidra-export-parity/bounded-forward.json)
and [reverse](ghidra-export-parity/bounded-reverse.json). Each report conforms
to `REVENGINSTR.GhidraExportParity/1`; both have equal SHA-256 values on the
left and right, `same_bytes: true`, `same_records: true`, and
`first_mismatch: null`. The TSVs contain program addresses and P-code rows and
remain local; only hashes and counts are committed.

For the shared forward exporter, invocation priority rules reproduced the
Nevergone fallback policy: default 3; ranks 0 for `EnemyActionsSystem` and
`ActionDataManager`; rank 1 for `EnemyObject`, `Enemy`, and `Action`; rank 2
for `FUN_`; rank 4 for `cocos2d::`, `std::`, and `__gnu_cxx::`.

For the shared reverse exporter, invocation priority rules reproduced the
Nevergone fallback policy: default 4; rank 0 for `EnemyObject`; rank 1 for
`EnemyActionsSystem` and `ActionDataManager`; rank 2 for `Enemy` and `Action`;
rank 3 for `FUN_`; rank 5 for `cocos2d::`, `std::`, and `__gnu_cxx::`.

This establishes parity for these bounded outputs, this root, these bounds,
and the equivalent priority policies. It does not establish parity for every
root or program; keep additional roots in the migration fixture set before
reducing the local fallbacks.
