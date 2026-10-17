# Bounded P-code exporter output parity

**Run date:** 2026-10-17  
**Ghidra:** `12.1.2_DEV`  
**Project:** `nevergone`  
**Program:** `libcocos2dcpp.so`  
**Mode:** `-readOnly -noanalysis`

The original forward/reverse comparison processed the same analyzed program
and root function, `EnemyActionsData::loadWBGFile` at `0029f430`. Its shared
scripts were taken from `revenginstr` at `a9bac53d`; the Nevergone fallback
sources were at `80a67f73`. A follow-up forward comparison added
`EnemyActionsSystem::filpAction` at `002bbd74`, using shared scripts from
`revenginstr` at `4105a763` and Nevergone fallbacks from consumer `main` at
`985bf294`. All runs used the same analyzed program in read-only, no-analysis
mode.

| Root | Export | Depth | Function limit | Functions | Rows | Cap reached | Record parity |
|---|---|---:|---:|---:|---:|---|---|
| `EnemyActionsData::loadWBGFile` (`0029f430`) | Bounded forward callgraph | 2 | 24 | 24 | 8,475 | yes | byte-identical |
| `EnemyActionsData::loadWBGFile` (`0029f430`) | Bounded reverse callgraph | 2 | 24 | 2 | 8,171 | no | byte-identical |
| `EnemyActionsSystem::filpAction` (`002bbd74`) | Bounded forward callgraph | 2 | 24 | 2 | 61 | no | byte-identical |

The machine reports are [first-root forward](ghidra-export-parity/bounded-forward.json),
[first-root reverse](ghidra-export-parity/bounded-reverse.json), and
[second-root forward](ghidra-export-parity/bounded-forward-second-root.json).
Each report conforms to `REVENGINSTR.GhidraExportParity/1`; compared outputs
have equal SHA-256 values, `same_bytes: true`, `same_records: true`, and
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

This establishes parity for these bounded outputs, these roots, these bounds,
and the equivalent priority policies. The second root currently covers the
forward direction only. It does not establish parity for every root or
program; keep expanding roots and bounds in the migration fixture set before
reducing the local fallbacks.
