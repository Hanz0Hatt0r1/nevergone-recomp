#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v9.jsonl"
QUERIES="$WORK_ROOT/static-receiver-vtable-queries-v9.jsonl"
CONVERGENCE="$WORK_ROOT/gamesceneui-controls-slot-convergence-audit.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v11.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v11.jsonl"
DETAILS="$WORK_ROOT/mixed-slot-target-recovery-v11.jsonl"
RESOLUTION="$WORK_ROOT/static-receiver-vtable-resolution-v11.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v11.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v11.txt"

for required in "$INFERENCE" "$QUERIES" "$CONVERGENCE" "$CATALOG" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_mixed_slot_target_recovery.py" \
  "$INFERENCE" "$QUERIES" "$CONVERGENCE" \
  --output "$OUT" \
  --queries-output "$OUTQ" \
  --details-output "$DETAILS"

echo
echo "receiver query resolution after mixed-slot target recovery:"
python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CATALOG" "$OUTQ" --output "$RESOLUTION"

echo
echo "virtual receiver failure audit after mixed-slot target recovery:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RECEIVER_TYPES" "$OUT" \
  --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

printf '\nv11 inference: %s\n' "$OUT"
printf 'v11 queries: %s\n' "$OUTQ"
printf 'v11 mixed-slot recovery details: %s\n' "$DETAILS"
printf 'v11 query resolution: %s\n' "$RESOLUTION"
printf 'v11 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
