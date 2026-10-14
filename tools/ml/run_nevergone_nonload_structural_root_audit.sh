#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

SHAPES="$WORK_ROOT/missing-receiver-shape-audit.jsonl"
TARGET="$WORK_ROOT/target-receiver-slot-provenance.tsv"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
DETAILS="$WORK_ROOT/nonload-structural-root-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/nonload-structural-root-audit.txt"

for required in "$SHAPES" "$TARGET" "$CATALOG"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -f "$REPO_ROOT/tools/ml/audit_nevergone_nonload_structural_roots.py" ]] || {
  echo "missing audit script" >&2
  exit 2
}

echo "== non-LOAD structural receiver root audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_nonload_structural_roots.py" \
  "$SHAPES" "$TARGET" "$CATALOG" \
  --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\nshape audit: %s\n' "$SHAPES"
printf 'target provenance: %s\n' "$TARGET"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
