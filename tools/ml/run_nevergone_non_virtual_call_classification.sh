#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${INFERENCE_V4:=$WORK_ROOT/static-receiver-vtable-inference-v4.jsonl}"
: "${PROVENANCE:=$REPO_ROOT/tools/ml/nevergone_non_virtual_call_provenance.tsv}"
: "${INFERENCE_V5:=$WORK_ROOT/static-receiver-vtable-inference-v5.jsonl}"
: "${RECEIVER_TYPES:=$WORK_ROOT/indirect-receiver-types.tsv}"
: "${FAILURES_V5:=$WORK_ROOT/receiver-type-failures-v5.jsonl}"
: "${FAILURE_AUDIT_V5:=$WORK_ROOT/receiver-type-failure-audit-v5.txt}"

[[ -f "$INFERENCE_V4" ]] || {
  echo "missing v4 inference: $INFERENCE_V4" >&2
  echo "run tools/ml/run_nevergone_receiver_provenance_fresh.sh first" >&2
  exit 2
}
[[ -f "$PROVENANCE" ]] || { echo "missing non-virtual provenance: $PROVENANCE" >&2; exit 2; }
[[ -f "$RECEIVER_TYPES" ]] || { echo "missing receiver types: $RECEIVER_TYPES" >&2; exit 2; }

python3 "$REPO_ROOT/tools/ml/classify_nevergone_non_virtual_indirect_calls.py" \
  "$INFERENCE_V4" "$PROVENANCE" \
  --output "$INFERENCE_V5"

echo
echo "virtual receiver failure audit after non-virtual classification:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" "$INFERENCE_V5" \
  --details-output "$FAILURES_V5" --samples 60 \
  | tee "$FAILURE_AUDIT_V5"

echo
printf 'v5 inference: %s\n' "$INFERENCE_V5"
printf 'v5 failure audit: %s\n' "$FAILURE_AUDIT_V5"
printf 'affects routing: no\n'
