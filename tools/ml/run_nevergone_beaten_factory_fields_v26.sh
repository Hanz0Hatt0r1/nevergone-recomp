#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

PCODE="$WORK_ROOT/evidence/protobuf-nested-recovery-v25"
SLICE="$WORK_ROOT/evidence/residual-closure-v24/slices/slice-002a47c6.tsv"
OUT="$WORK_ROOT/beaten-factory-field-audit-v26.json"

for input in "$PCODE/pcode-002a3a20.tsv" "$PCODE/pcode-002a3c74.tsv" "$SLICE"; do
  [[ -f "$input" ]] || { echo "missing required input: $input" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/nevergone_beaten_factory_fields_v26_smoke.py"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_factory_fields_v26.py" \
  "$PCODE" "$SLICE" --output "$OUT"

echo 'BeatenEffect receiver classes remain unresolved pending alias and writer closure.'
