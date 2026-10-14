# Caller argument subtree probe

This evidence-only probe targets unresolved `receiver_not_single_pointer` rows whose structural receiver is rooted in a callee parameter and whose direct caller arguments lacked a one-level pointer class under the older shallow transparent-flow analysis.

The Ghidra exporter labels SSA edges so downstream analysis can distinguish value-preserving `COPY`/`CAST`/`INDIRECT`/`MULTIEQUAL` flow from `LOAD` address, pointer base, and offset flow. Only the pure value-equivalent subtree is eligible for catalog class mapping; address/base types are diagnostic only.

The current real-data target is the three `002a7594` calls that already have complete matched direct-caller coverage but no shallow pointer class. The probe itself does not alter inference, queries, or routing.

## Fail-closed v10 recovery

`tools/ml/finalize_nevergone_caller_argument_subtree_recovery.py` is a separate post-finalizer. It can promote a v9 `receiver_not_single_pointer` row only when all of the following independently agree:

- the persisted caller-subtree plan still matches the unresolved source call and dispatch slot;
- the audit reports `complete_caller_argument_subtrees` with the exact planned caller/matched-call counts;
- every matched direct-caller argument has exactly the same single catalog class on the value-preserving subtree;
- the aggregate audit status is `all_caller_arguments_converge_one_value_class`;
- that class maps to exactly one `receiver_vtable_id` in the conservative static catalog.

Incomplete caller coverage, mixed/empty value classes, LOAD address/base-only typing, audit-detail disagreement, plan drift, and ambiguous class-to-vtable mappings remain unresolved. Accepted rows are written to v10 inference/query outputs and continue to carry `affects_routing: false`; the recovery does not infer from function names, target slots, sibling calls, or address proximity.

Run the probe first, then apply the bounded recovery with:

```bash
bash tools/ml/run_nevergone_caller_argument_subtree_probe.sh
bash tools/ml/run_nevergone_caller_argument_subtree_recovery.sh
```
