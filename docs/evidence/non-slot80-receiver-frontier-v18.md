# P1A: post-v17 non-slot-0x80 receiver frontier v18

## Why this stage exists

The receiver pipeline now has separate, fail-closed recovery stages for every
previously known slot-`0x80` group. v12 and v13 also recovered three
non-slot-`0x80` `receiver_not_single_pointer` rows before that work:

- v12: two exact singleton-cell receivers at slot `0x8`;
- v13: one `CCJoyStick` field receiver at slot `0x58`.

The conservative topology before those stages had 30 unresolved virtual calls.
The expected progression is therefore:

| Stage | Expected unresolved virtual rows | Change |
| --- | ---: | ---: |
| corrected v11 | 30 | baseline |
| v12 | 28 | -2 |
| v13 | 27 | -1 |
| v14 | 24 | -3 |
| v15 | 13 | -11 |
| v16 | 10 | -3 |
| v17 | 7 | -3 |

The final `7` is an **expected real-data invariant**, not a claim that v17 has
already been rerun locally in this PR. The v18 runner enforces that invariant
against regenerated/user-held v17 artifacts and fails closed if the real
frontier differs.

All expected post-v17 rows are `unresolved_receiver_class` on slots other than
`0x80`. v18 does not guess their classes. Instead it captures the actual
structural receiver value for each remaining CALLIND so the next proof can be
chosen from evidence.

## Planner gate

`plan_nevergone_non_slot80_receiver_frontier_v18.py` consumes:

- `receiver-type-failures-v17.jsonl`;
- `static-receiver-vtable-inference-v17.jsonl`.

For every failure row it requires exact inference agreement on source/call,
call address, status and normalized slot. It rejects:

- any remaining slot `0x80` failure;
- any post-v17 non-slot failure status other than
  `unresolved_receiver_class`;
- an unresolved-class row that already carries receiver-class candidates;
- missing/invalid slots or addresses;
- duplicate or unmatched failure identities.

The end-to-end runner additionally requires exactly **7** selected calls. This
turns the historical stage arithmetic into a topology-change alarm: new or
missing rows stop the probe instead of being silently ignored.

Output format:

`NEVERGONE.NonSlot80ReceiverFrontierPlan/1`

The TSV keeps source identity plus aligned call indices, call addresses and
slot offsets. It remains evidence-only.

## Structural receiver audit

For each planned source function,
`ExportIndirectCallStructuralReceiverSlice.java` walks the receiver recovered
from the validated virtual-dispatch shape
`CALLIND(LOAD(LOAD(receiver) + constant_slot))`.

`audit_nevergone_non_slot80_receiver_frontier_v18.py` then requires, per
selected call:

- exact function/call/slot identity;
- `canonical_receiver` recovery;
- one summary and one root node;
- no node-budget truncation, depth truncation or cycles;
- consistent node counts and parent identities;
- no truncated nodes.

A complete row records the root opcode/type, node opcode histogram, constants
seen in the bounded slice and direct CALL roots. It assigns only a
**next proof family**:

| Root opcode | Next proof family |
| --- | --- |
| `LOAD` | `receiver_load_write_provenance` |
| `CALL` | `receiver_call_return_provenance` |
| `MULTIEQUAL` | `receiver_phi_input_provenance` |
| `PTRSUB/PTRADD/INT_ADD/INT_SUB` | `receiver_address_expression_provenance` |
| no defining op | `receiver_input_provenance` |
| anything else | `receiver_structural_provenance` |

This classification is workflow routing only. Every v18 audit row explicitly
keeps `receiver_class_proven=false` and `affects_routing=false`; it does not
emit a receiver-vtable query or modify production inference.

## Run after real v17 evidence is available

First generate the v17 artifacts with the exact APK/evidence inputs used by the
v17 runner. Then:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only

bash tools/ml/run_nevergone_non_slot80_receiver_frontier_v18.sh
```

The runner reads v17 artifacts from:

`$HOME/.cache/nevergone-recomp/receiver-provenance/`

It creates bounded read-only Ghidra receiver slices with a 160-node / 24-depth
budget, requires all seven slices to be complete, and writes:

- `non-slot80-receiver-frontier-v18-plan.tsv`;
- `evidence/non-slot80-receiver-frontier-v18/*.tsv`;
- `non-slot80-receiver-frontier-v18-audit.jsonl`;
- `non-slot80-receiver-frontier-v18-audit.txt`.

Archive:

`~/nevergone-non-slot80-receiver-frontier-v18.tar.gz`

The archive contains evidence metadata only; no APK/ELF bytes or decompiler C
text are added to the repository.

## Next development step

The real v18 audit will tell us which of the seven rows should enter which
provenance family. The next recovery stage should be implemented against the
largest repeated root shape first, with an independent exact class/vtable/slot
join and the same fail-closed query identity checks used by v12-v17.
