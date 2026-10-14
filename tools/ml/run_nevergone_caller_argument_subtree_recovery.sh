#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v9.jsonl"
QUERIES="$WORK_ROOT/static-receiver-vtable-queries-v9.jsonl"
PLAN="$WORK_ROOT/caller-argument-subtree-plan.tsv"
AUDIT="$WORK_ROOT/caller-argument-subtree-audit.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v10.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v10.jsonl"
DETAILS="$WORK_ROOT/caller-argument-subtree-recovery-v10.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v10.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v10.txt"

for required in "$INFERENCE" "$QUERIES" "$PLAN" "$AUDIT" "$CATALOG" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_caller_argument_subtree_recovery.py" \
  "$INFERENCE" "$QUERIES" "$PLAN" "$AUDIT" "$CATALOG" \
  --output "$OUT" \
  --queries-output "$OUTQ" \
  --details-output "$DETAILS"

echo
echo "virtual receiver failure audit after caller-argument subtree recovery:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" "$OUT" \
  --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

printf '\nv10 inference: %s\n' "$OUT"
printf 'v10 queries: %s\n' "$OUTQ"
printf 'v10 caller-argument recovery details: %s\n' "$DETAILS"
printf 'v10 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
