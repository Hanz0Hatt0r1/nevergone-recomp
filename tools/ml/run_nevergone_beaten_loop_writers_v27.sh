#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

PCODE="$WORK_ROOT/evidence/protobuf-nested-recovery-v25"
DTOR="$WORK_ROOT/evidence/residual-closure-v24/pcode/pcode-002a3908.tsv"
SLICE="$WORK_ROOT/evidence/residual-closure-v24/slices/slice-002a47c6.tsv"
FLOW="$WORK_ROOT/owner-alias-flow-v23-beateneffect.tsv"
OUT="$WORK_ROOT/beaten-loop-writer-audit-v27.json"

for input in "$PCODE/pcode-002a3a20.tsv" "$PCODE/pcode-002a3c74.tsv" "$DTOR" "$SLICE" "$FLOW"; do
  [[ -f "$input" ]] || { echo "missing required input: $input" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_loop_writers_v27.py" \
  "$PCODE" "$DTOR" "$SLICE" "$FLOW" --output "$OUT"
