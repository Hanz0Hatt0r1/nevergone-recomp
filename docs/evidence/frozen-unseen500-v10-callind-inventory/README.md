# Frozen unseen500 v10 CALLIND inventory

This is the evidence-backed acquisition pass for the 500-function
identity-disjoint `unseen500-v10` holdout. It adds exact High-P-code call-site
records to the existing receiver-coverage snapshot without assigning virtual
call semantics to raw `CALLIND` operations.

## Results

| Measurement | Result |
| --- | ---: |
| Canonical functions in scope | 500 |
| Complete High-P-code flow exports | 500 / 500 |
| Canonical `CALLIND` count | 203 |
| Observed `CALLIND` sites | 203 |
| Functions with exact count reconciliation | 500 / 500 |
| Sites with target-expression rows | 203 / 203 |
| Expressions with a depth-limit marker | 140 |
| Expressions with a cycle marker | 0 |
| Canonical corpus records marked truncated | 24 |
| Site classifications | 203 `unknown` |

The 24 canonical truncation flags describe the normalized model corpus. The
separate High-P-code flow export completed for all 500 functions. The 140
depth-limit markers mean those target expressions are bounded and incomplete;
their rows remain visible in the inventory. This holdout has no independent
semantic labels, so this report gives no virtual-call recall, precision, or
accuracy result.

## Pinned artifacts

`replay-manifest.json` records hashes for the population adapter, exact function
list, P-code flow TSV, indirect-call expression JSONL, acquisition statuses,
inventory output, and summary. Each inventory row retains the exact source
hash and record index for its population, P-code, and expression evidence.
`canonical_source` points to the frozen corpus SHA-256 already recorded in the
parent receiver-coverage report. The generic implementation is pinned to
[RevEngInstr commit `252d4bd`](https://github.com/Hanz0Hatt0r1/revenginstr/commit/252d4bd5ae8f2c11d3556c5200a70c37f9497208).

The Ghidra run used `12.1.2-DEV` with Java `21.0.12.1`, `-readOnly`, and
`-noanalysis`. It selected the 500 exact entries in `function-entries.txt`,
with a 500-function cap, a 1,800-second batch cap, and a 60-second timeout per
decompilation. The per-function acquisition sidecar reports 500 complete,
zero no-P-code, and zero decompile-failed functions. The manifest pins the
selected project tree and `libcocos2dcpp.so` content hashes; neither binary nor
Ghidra project files are included here.

## Reproduction

From the repository root, use the pinned RevEngInstr checkout and the local
read-only Ghidra project whose content hash matches the manifest:

```bash
GHIDRA_HOME=/opt/ghidra JAVA_HOME=/usr/lib/jvm/java-21-openjdk \
  scripts/run_ghidra_pcode_flow_batch.sh \
  /path/to/projects nevergone libcocos2dcpp.so \
  docs/evidence/frozen-unseen500-v10-callind-inventory/function-entries.txt \
  docs/evidence/frozen-unseen500-v10-callind-inventory/high-pcode-flow.tsv \
  docs/evidence/frozen-unseen500-v10-callind-inventory/callind-target-expressions.jsonl \
  docs/evidence/frozen-unseen500-v10-callind-inventory/pcode-acquisition.jsonl 500 1800

python tools/build_callind_population.py \
  /path/to/unseen500-v10/corpus.jsonl \
  docs/evidence/frozen-unseen500-v10-receiver-coverage/source-manifest.json \
  docs/evidence/frozen-unseen500-v10-callind-inventory/pcode-acquisition.jsonl \
  docs/evidence/frozen-unseen500-v10-callind-inventory/callind-population.jsonl

revenginstr-callind-inventory \
  docs/evidence/frozen-unseen500-v10-callind-inventory/callind-population.jsonl \
  docs/evidence/frozen-unseen500-v10-callind-inventory/callind-inventory.jsonl \
  --pcode-flow docs/evidence/frozen-unseen500-v10-callind-inventory/high-pcode-flow.tsv \
  --indirect-call-evidence docs/evidence/frozen-unseen500-v10-callind-inventory/callind-target-expressions.jsonl \
  --summary docs/evidence/frozen-unseen500-v10-callind-inventory/callind-summary.json
```

The canonical corpus is not copied into this repository. The adapter checks it
against the pinned source manifest before writing `CallindPopulation/1`; it
preserves the original corpus format, artifact hash, and record index.
