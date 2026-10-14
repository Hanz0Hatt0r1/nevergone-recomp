# GameSceneUI / Controls mixed receiver slot convergence

## Scope

This evidence slice targets the three still-unresolved indirect calls in `libcocos2dcpp.so:002a7594` at call indices 3, 4 and 7. The retained caller-subtree evidence shows a mixed receiver shape: some branches are the direct result of `GameSceneUI::sharedGameSceneUI()`, while others load the object field at `sharedGameSceneUI() + 0x134`. All three indirect calls use dispatch slot `0x80`.

The goal is deliberately narrower than recovering one receiver class. The probe asks whether every class independently proven for those mixed branches has the same exact vtable-slot target.

## Independent class provenance

The singleton branch is not typed from the `sharedGameSceneUI` function name. `ExportSingletonStoreReturnProvenance.java` records the function's direct constructor call, the constructor-rooted object stored into the singleton cell and the return load from that same cell. `audit_nevergone_singleton_store_return.py` accepts the branch as `GameSceneUI` only when the constructor/store/return relation is unique and the static catalog maps that class to exactly one vtable.

### Identity-gated Thumb return-cell witness

The first real Ghidra audit of the original APK reported one constructor call, one constructor-rooted singleton store, and one return, but rejected the return as `return_not_from_constructor_singleton_cell`. The high-P-code address roots did not join. This is **not** evidence that the machine instructions return a different object.

For the original `libcocos2dcpp.so` SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`, the Thumb code at ELF `0x3d95e0` (Ghidra `003e95e0`) has a 48-byte function slice with SHA-256 `ceb4bb6eb7801f478a82b5d83fa1932c28d9fb202951d3929ea1e041b2720907`. Independent instruction inspection shows the constructor-rooted object is stored through `r5`, and the return loads through `r3`. The two PC-relative literals compute the same Ghidra cell address `0x94efd0` (ELF `0x93efd0`).

The exporter now emits `thumb_exact_cell_proof` only when **all 48 bytes** match that retained SHA-256, both decoded literal addresses agree with the expected singleton cell, and the cell exists in Ghidra memory. The auditor may use this narrowly scoped machine-level witness when high-P-code return aliasing differs, but only if there is exactly one constructor call, one constructor-rooted store, no competing store to that cell, and exactly one return. A missing/tampered witness or a different binary fails closed. The witness preserves the existing high-P-code rows and records `return_provenance_method` in the audit.

The resulting `GameSceneUI_evidence_catalog_vtable_mismatch` in the mixed-target audit was downstream of the failed singleton audit: the failed audit emitted no candidate vtable, while the catalog had the unique `GameSceneUI` vtable. The machine witness does not change the catalog or slot targets.

The `+0x134` branch reuses the independently merged accessor-consumer evidence. The corrected `GetControlsObj` probe must first prove that the accessor returns exactly `this + 0x134`. `ExportAccessorReturnConsumers.java` then follows only value-preserving uses of that exact return value and records class evidence only when the value is passed as `this` to a resolved direct member call. `audit_nevergone_gamesceneui_controls_return_consumers.py` must report `unique_catalog_member_consumer_class` with `mapped_classes == ["Controls"]` and exactly one Controls vtable. The accessor name itself is never treated as class evidence.

## Slot convergence

After both branch classes are independently proven, `audit_nevergone_gamesceneui_mixed_receiver_target.py` looks up only the exact `(receiver_vtable_id, 0x80)` keys in the conservative static vtable catalog. It succeeds only when:

- `GameSceneUI` has exactly one catalog vtable and it matches the singleton audit;
- `Controls` has exactly one catalog vtable and it matches the return-consumer audit;
- neither exact slot row is already marked `runtime_mismatch`;
- each exact slot has exactly one static candidate target; and
- the two candidate-target sets are identical singletons.

A successful row uses `NEVERGONE.MixedReceiverSlotTargetConvergenceAudit/1` with status `unique_target_across_proven_receiver_classes`. It retains both receiver classes and both vtables; it does **not** collapse the mixed PHI to one class.

## Fail-closed boundary

Missing singleton constructor provenance, a non-unique singleton store, incomplete accessor/consumer evidence, mixed or unmapped consumer classes, incomplete caller coverage, class/vtable ambiguity, an exact-slot runtime mismatch, or divergent slot targets all produce `mixed_receiver_target_not_proven` with explicit reasons.

This probe is evidence-only. It does not alter receiver inference, query emission or production routing. A later finalizer may consume the retained convergence result only after real Ghidra data confirms the complete chain.

## v11 recovery provenance gate

The older `GameSceneUIControlsSlotConvergenceAudit/1` establishes that the two catalog vtable slots share a target, but it does **not** independently prove that `sharedGameSceneUI()` returns a `GameSceneUI` object. The v11 post-finalizer must therefore consume **both** that slot audit and all three `MixedReceiverSlotTargetConvergenceAudit/1` per-call rows. Every strict row must be accepted, have the exact source/call/slot, preserve both proven classes and their vtables, and agree on the same target. Missing or failed singleton provenance blocks v11 recovery even if the catalog targets match.

After running the mixed receiver probe with the corrected singleton exporter, run:

```bash
bash tools/ml/run_nevergone_gamesceneui_controls_slot_convergence_audit.sh
bash tools/ml/run_nevergone_mixed_slot_target_recovery_v11.sh
```

The v11 output may then mark calls 3/4/7 `resolved_converged_target`, retaining both possible receiver classes and both vtables. This remains evidence-only and does not change production routing.
