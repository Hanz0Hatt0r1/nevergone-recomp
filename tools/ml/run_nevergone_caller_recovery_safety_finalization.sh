#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v5.jsonl"
QUERIES="$WORK_ROOT/static-receiver-vtable-queries-v4.jsonl"
SAFETY="$WORK_ROOT/resolved-caller-safety-audit.jsonl"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v6.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v6.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
FAILURES="$WORK_ROOT/receiver-type-failures-v6.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v6.txt"

for required in "$INFERENCE" "$QUERIES" "$SAFETY" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/enforce_nevergone_caller_recovery_safety.py" \
  "$INFERENCE" "$QUERIES" "$SAFETY" \
  --output "$OUT" \
  --queries-output "$OUTQ"

echo
echo "virtual receiver failure audit after caller safety finalization:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" "$OUT" \
  --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

printf '\nv6 inference: %s\n' "$OUT"
printf 'v6 queries: %s\n' "$OUTQ"
printf 'v6 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
