#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v7.jsonl"
QUERIES="$WORK_ROOT/static-receiver-vtable-queries-v7.jsonl"
V7_DETAILS="$WORK_ROOT/phi-default-instance-recovery-v7.jsonl"
SLICE_DIR="$WORK_ROOT/evidence/structural-receiver-slices"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v8.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v8.jsonl"
DETAILS="$WORK_ROOT/phi-primary-subtree-recovery-v8.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v8.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v8.txt"

for required in "$INFERENCE" "$QUERIES" "$V7_DETAILS" "$CATALOG" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -d "$SLICE_DIR" ]] || { echo "missing structural receiver slice dir: $SLICE_DIR" >&2; exit 2; }

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_phi_primary_subtree_recovery.py" \
  "$INFERENCE" \
  "$QUERIES" \
  "$V7_DETAILS" \
  "$SLICE_DIR" \
  "$CATALOG" \
  --output "$OUT" \
  --queries-output "$OUTQ" \
  --details-output "$DETAILS"

echo
echo "virtual receiver failure audit after PHI primary-subtree recovery:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" "$OUT" \
  --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

printf '\nv8 inference: %s\n' "$OUT"
printf 'v8 queries: %s\n' "$OUTQ"
printf 'v8 PHI primary-subtree details: %s\n' "$DETAILS"
printf 'v8 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
