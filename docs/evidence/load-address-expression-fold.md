# LOAD address-expression fold evidence

This probe addresses the remaining `receiver_not_single_pointer` cases whose canonical receiver is a `LOAD` from a non-parameter address expression.

The earlier address-root classification stops at the terminal literal leaf. That is intentionally not treated as the receiver cell because the address subtree can contain `INT_ADD`, `INT_SUB`, `PTRADD`, `PTRSUB`, transparent copies/casts, nested `LOAD`, or convergent `MULTIEQUAL` nodes.

`tools/ml/audit_nevergone_load_address_expression_fold.py` evaluates only the bounded address-input subtree of the canonical receiver `LOAD`. Memory reads are permitted only when an exact classified `raw_u32` word is available. Unsupported operations, missing memory words, cycles, and divergent PHIs fail closed.

When an exact effective address is folded, the runner classifies that address and its raw value with Ghidra and collects direct xref owners using `ExportAddressReferenceOwners.java`. These artifacts remain evidence-only and do not infer a receiver class, emit a receiver query, or affect routing.
