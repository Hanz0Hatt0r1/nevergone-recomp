#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

FLOW="$WORK_ROOT/owner-alias-flow-v23-beateneffect.tsv"
SLICE="$WORK_ROOT/evidence/residual-closure-v24/slices/slice-002a3e94.tsv"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
PCODE="$WORK_ROOT/evidence/residual-closure-v24/pcode/pcode-002a3e94.tsv"
OUT="$WORK_ROOT/beaten-owner-call-audit-v28.json"

for input in "$FLOW" "$SLICE" "$CATALOG" "$PCODE"; do
  [[ -f "$input" ]] || { echo "missing required input: $input" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_owner_calls_v28.py" \
  "$FLOW" "$SLICE" "$CATALOG" "$PCODE" --output "$OUT"
