# Frozen unseen500 PointerProvenance replay

This consumer replay applies `PointerProvenance/1` to the same 500-function
`unseen500-v10` High-P-code and `CALLIND` evidence pinned by the companion
[CALLIND inventory replay](../frozen-unseen500-v10-callind-inventory/README.md).
The manifest pins both inputs, the baseline replay manifest, the RevEngInstr
commit, the analyzer and schema hashes, and the filtered 203-row result.

## Results

| Measurement | Result |
| --- | ---: |
| Functions in the frozen population | 500 |
| High-P-code flow rows | 40,519 |
| Exact `CALLIND` target joins | 203 / 203 |
| Target provenance: memory-read anchors | 191 |
| Target provenance: resolved constant address | 1 |
| Target provenance: ambiguous | 2 |
| Target provenance: side-effect barrier | 8 |
| Target provenance: unknown | 1 |
| Baseline inventory classifications | 203 `unknown` |
| Virtual-dispatch identities or accuracy claims | 0 |

`memory_read_anchor` means the target value is structurally produced by a
P-code `LOAD`; the exact P-code rows remain linked by the manifest and evidence
references. It does not identify an object, vtable, slot, or runtime target.
The constant-address row is an address observation, not proof that the site is
a virtual call. Ambiguous, barrier, and unknown cases remain visible.

## Reproduction

From the Nevergone repository root, with the pinned RevEngInstr checkout
installed, run the analyzer over the complete source artifacts and filter its
JSONL output to exact call-target rows:

```sh
revenginstr-pointer-provenance \
  --program-id 'libcocos2dcpp.so' \
  --pcode-flow docs/evidence/frozen-unseen500-v10-callind-inventory/high-pcode-flow.tsv \
  --indirect-call-evidence docs/evidence/frozen-unseen500-v10-callind-inventory/callind-target-expressions.jsonl \
  --output /tmp/pointer-provenance.jsonl \
  --max-operations 50000

python - <<'PY'
import json
from pathlib import Path

source = Path('/tmp/pointer-provenance.jsonl')
destination = Path('docs/evidence/frozen-unseen500-v10-pointer-provenance/callind-target-provenance.jsonl')
rows = (json.loads(line) for line in source.read_text().splitlines() if line.strip())
destination.write_text(''.join(
    json.dumps(row, sort_keys=True) + '\n'
    for row in rows if row['record_kind'] == 'callind_target'
), encoding='utf-8')
PY
```

The output contains only the 203 `callind_target` rows; the full run emitted
35,575 operation and target rows. The manifest pins RevEngInstr commit
[`a211662`](https://github.com/Hanz0Hatt0r1/revenginstr/commit/a211662077650dbfc5c24ab7b1b80de85e99142d).
