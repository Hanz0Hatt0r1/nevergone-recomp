#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v6.jsonl"
QUERIES="$WORK_ROOT/static-receiver-vtable-queries-v6.jsonl"
SLICE_DIR="$WORK_ROOT/evidence/structural-receiver-slices"
PHI_AUDIT="$WORK_ROOT/phi-secondary-load-tree-audit.jsonl"
GOT_CLASSIFICATION="$WORK_ROOT/phi-secondary-resolved-address-classification.tsv"
GOT_TARGET_CLASSIFICATION="$WORK_ROOT/phi-secondary-got-target-classification.tsv"
OWNER_REFERENCES="$WORK_ROOT/phi-default-instance-owner-references.tsv"
TARGET_PROVENANCE="$WORK_ROOT/target-receiver-slot-provenance.tsv"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v7.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v7.jsonl"
DETAILS="$WORK_ROOT/phi-default-instance-recovery-v7.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v7.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v7.txt"

for required in \
  "$INFERENCE" "$QUERIES" "$PHI_AUDIT" "$GOT_CLASSIFICATION" \
  "$GOT_TARGET_CLASSIFICATION" "$OWNER_REFERENCES" "$TARGET_PROVENANCE" \
  "$CATALOG" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -d "$SLICE_DIR" ]] || { echo "missing structural receiver slice dir: $SLICE_DIR" >&2; exit 2; }

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_phi_default_instance_recovery.py" \
  "$INFERENCE" \
  "$QUERIES" \
  "$SLICE_DIR" \
  "$PHI_AUDIT" \
  "$GOT_CLASSIFICATION" \
  "$GOT_TARGET_CLASSIFICATION" \
  "$OWNER_REFERENCES" \
  "$TARGET_PROVENANCE" \
  "$CATALOG" \
  --output "$OUT" \
  --queries-output "$OUTQ" \
  --details-output "$DETAILS"

echo
echo "virtual receiver failure audit after PHI default-instance recovery:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" "$OUT" \
  --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

printf '\nv7 inference: %s\n' "$OUT"
printf 'v7 queries: %s\n' "$OUTQ"
printf 'v7 PHI recovery details: %s\n' "$DETAILS"
printf 'v7 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
