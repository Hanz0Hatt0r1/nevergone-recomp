#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v8.jsonl"
QUERIES="$WORK_ROOT/static-receiver-vtable-queries-v8.jsonl"
PLAN="$WORK_ROOT/call-root-factory-return-plan.tsv"
AUDIT="$WORK_ROOT/call-root-factory-return-audit.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v9.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v9.jsonl"
DETAILS="$WORK_ROOT/call-root-factory-recovery-v9.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v9.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v9.txt"

for required in "$INFERENCE" "$QUERIES" "$PLAN" "$AUDIT" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_call_root_factory_recovery.py" \
  "$INFERENCE" \
  "$QUERIES" \
  "$PLAN" \
  "$AUDIT" \
  --output "$OUT" \
  --queries-output "$OUTQ" \
  --details-output "$DETAILS"

echo
echo "virtual receiver failure audit after CALL-root factory recovery:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" "$OUT" \
  --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

printf '\nv9 inference: %s\n' "$OUT"
printf 'v9 queries: %s\n' "$OUTQ"
printf 'v9 CALL-root factory recovery details: %s\n' "$DETAILS"
printf 'v9 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
