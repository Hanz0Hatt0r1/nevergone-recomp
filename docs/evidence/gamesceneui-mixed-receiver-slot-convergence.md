# GameSceneUI / Controls mixed receiver slot convergence

## Scope

This evidence slice targets the three still-unresolved indirect calls in `libcocos2dcpp.so:002a7594` at call indices 3, 4 and 7. The retained caller-subtree evidence shows a mixed receiver shape: some branches are the direct result of `GameSceneUI::sharedGameSceneUI()`, while others load the object field at `sharedGameSceneUI() + 0x134`. All three indirect calls use dispatch slot `0x80`.

The goal is deliberately narrower than recovering one receiver class. The probe asks whether every class independently proven for those mixed branches has the same exact vtable-slot target.

## Independent class provenance

The singleton branch is not typed from the `sharedGameSceneUI` function name. `ExportSingletonStoreReturnProvenance.java` records the function's direct constructor call, the constructor-rooted object stored into the singleton cell and the return load from that same cell. `audit_nevergone_singleton_store_return.py` accepts the branch as `GameSceneUI` only when the constructor/store/return relation is unique and the static catalog maps that class to exactly one vtable.

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
