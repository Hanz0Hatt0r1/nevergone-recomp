# P1A: unresolved slot `0x80` receiver slices (v11)

## Input evidence (2026-10-14)

A user-provided v11 failure export contained 62 inference rows, 27 unresolved
virtual calls, and 17 still-unresolved slot `0x80` calls in exactly five
functions:

| Source | Calls | Status |
| --- | --- | --- |
| `libcocos2dcpp.so:002a74c0` | 0, 1, 2, 3 | unresolved receiver class |
| `libcocos2dcpp.so:002a7594` | 0, 1, 2, 5, 6 | unresolved receiver class |
| `libcocos2dcpp.so:002a7696` | 0, 1 | unresolved receiver class |
| `libcocos2dcpp.so:002a78cc` | 0, 1, 2 | unresolved receiver class |
| `libcocos2dcpp.so:002a87d2` | 3, 4, 5 | ambiguous receiver class |

Three other calls in `002a7594` (3, 4, 7) were already independently
resolved by strict GameSceneUI/Controls target convergence; this probe must
**not** reuse their target across the remaining five calls without new
receiver-value evidence.

For the three ambiguous calls, v11 caller-type evidence lists both
`ControlsButton` and `cocos2d::CCTouch`. The vtable catalog contains one
`ControlsButton` slot `0x80` target (`libcocos2dcpp.so:00525d2e`), but
`CCTouch` has only nine observed contiguous primary slots, ending at `0x20`.
The missing `CCTouch` slot is a **feasibility clue, not proof** that the
receiver is a `ControlsButton`; the candidate may originate from another
caller argument.

## Run (Manjaro, local Ghidra)

```bash
cd `/nevergone-recomp
git switch main
git pull --ff-only
bash tools/ml/run_nevergone_slot80_receiver_slice_probe.sh
```

The runner plans the exact current v11 failures, requires 17 calls across five
source functions, runs the existing read-only Ghidra structural receiver
exporter with 128 nodes / 20 depth, then audits each call's address, source,
slot, root, node count, truncation and candidate class-slot feasibility.
It refuses missing/stale/incomplete structural slices.

Output in `$HOME/.cache/nevergone-recomp/receiver-provenance/`:

- `slot80-receiver-slice-plan.tsv`
- `evidence/slot80-receiver-slices/*.tsv`
- `slot80-receiver-slice-audit.jsonl`
- `slot80-receiver-slice-audit.txt`

Package the audit and raw slices for review:

```bash
WORK="$HOME/.cache/nevergone-recomp/receiver-provenance"
tar -C "$WORK" -czf "$HOME/nevergone-slot80-receiver-evidence.tar.gz" \
  slot80-receiver-slice-plan.tsv \
  slot80-receiver-slice-audit.jsonl \
  slot80-receiver-slice-audit.txt \
  evidence/slot80-receiver-slices
```

The planner/auditor are evidence-only: no class inference is accepted and
`affects_routing` remains `false`. Synthetic smoke covers missing slices,
call-address tampering, inference/failure mismatch, and catalog prefix bounds.
