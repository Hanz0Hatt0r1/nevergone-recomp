# Exact singleton-cell receiver recovery v12

## Scope

This stage consumes the evidence-only `NEVERGONE.ExactSingletonCellProvenanceAudit/1` produced for the two remaining slot-`0x8` singleton receivers:

- `libcocos2dcpp.so:0029edb4` call `0` -> exact `.bss` cell `0x0094cb44`.
- `libcocos2dcpp.so:002a08fc` call `1` -> exact `.bss` cell `0x0094cbd8`.

The accepted local evidence proves, independently for each row:

1. a class-qualified constructor receives an object root as `this`;
2. that exact object root is stored to the independently identified singleton cell;
3. the same exact cell is loaded by the singleton accessor;
4. the exact-cell LOAD root reaches RETURN;
5. the resulting class maps to one catalog vtable;
6. the unresolved dispatch slot has one catalog target for that vtable.

Observed evidence:

| Receiver row | Proven class | Vtable | Slot | Exact target |
| --- | --- | --- | --- | --- |
| `0029edb4` call `0` | `ActionDataManager` | `libcocos2dcpp.so:008f3988` | `0x8` | `libcocos2dcpp.so:0029eb38` (`ActionDataManager::~ActionDataManager`) |
| `002a08fc` call `1` | `AndroidSdkMgr` | `libcocos2dcpp.so:008f3ce8` | `0x8` | `libcocos2dcpp.so:002a0d8c` (`AndroidSdkMgr::~AndroidSdkMgr`) |

## v12 finalizer

`tools/ml/finalize_nevergone_exact_singleton_cell_recovery.py` is a narrow post-finalizer over the current v11 baseline. It accepts only the two keys above. It does not infer a class from accessor names or from `nearby_class_evidence`; those names are identity gates for the already proven exact-cell owner only.

Before recovery, the finalizer re-checks the accepted singleton audit against `StaticVtableSlotCatalog/3`: class, unique vtable, exact slot, single-function static status, target identity, and absence of runtime mismatch must all agree. Accepted rows become ordinary `resolved_unique_vtable` rows and emit normal exact-vtable queries.

`tools/ml/run_nevergone_exact_singleton_cell_recovery_v12.sh` first regenerates v11 through the current quarantine logic. This prevents stale pre-#568 artifacts from reviving `002a7594` calls `3/4/7`. Those three calls remain `receiver_not_single_pointer` in v12.

Expected real-data effect from the current conservative v11 baseline:

- queries: `30 -> 32`;
- unresolved virtual rows: `30 -> 28`;
- `receiver_not_single_pointer`: `6 -> 4`;
- quarantined `002a7594` calls `3/4/7`: unchanged and unresolved.

No cross-vtable transfer or sibling/neighbor propagation is introduced. `affects routing: no` remains explicit throughout the evidence and resolution path.
