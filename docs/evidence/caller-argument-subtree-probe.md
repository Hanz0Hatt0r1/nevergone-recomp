# Caller argument subtree probe

This evidence-only probe targets unresolved `receiver_not_single_pointer` rows whose structural receiver is rooted in a callee parameter and whose direct caller arguments lacked a one-level pointer class under the older shallow transparent-flow analysis.

The Ghidra exporter labels SSA edges so downstream analysis can distinguish value-preserving `COPY`/`CAST`/`INDIRECT`/`MULTIEQUAL` flow from `LOAD` address, pointer base, and offset flow. Only the pure value-equivalent subtree is eligible for catalog class mapping; address/base types are diagnostic only.

The current real-data target is the three `002a7594` calls that already have complete matched direct-caller coverage but no shallow pointer class. The probe does not alter inference, queries, or routing.
