#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${MAX_NODES:=128}"
: "${MAX_DEPTH:=20}"

FAILURES="$WORK_ROOT/receiver-type-failures-v11.jsonl"
INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v11.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
PLAN="$WORK_ROOT/slot80-receiver-slice-plan.tsv"
SLICE_DIR="$WORK_ROOT/evidence/slot80-receiver-slices"
DETAILS="$WORK_ROOT/slot80-receiver-slice-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/slot80-receiver-slice-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in "$FAILURES" "$INFERENCE" "$CATALOG" \
  "$REPO_ROOT/tools/ml/plan_nevergone_slot80_receiver_slices.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_slot80_receiver_slices.py" \
  "$REPO_ROOT/tools/ghidra/ExportIndirectCallStructuralReceiverSlice.java"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$SLICE_DIR"

python3 "$REPO_ROOT/tools/ml/plan_nevergone_slot80_receiver_slices.py" \
  "$FAILURES" "$INFERENCE" --output "$PLAN" --expected-calls 17 --expected-sources 5

while IFS=$'\t' read -r format source entry calls addresses expected; do
  [[ "$format" == "NEVERGONE.Slot80ReceiverSlicePlan/1" ]] || { echo "invalid plan format" >&2; exit 2; }
  [[ "$entry" =~ ^[0-9a-f]{8}$ ]] || { echo "invalid entry: $entry" >&2; exit 2; }
  safe="${source//\//_}"
  safe="${safe//:/_}"
  out="$SLICE_DIR/$safe.tsv"
  printf '\n== Ghidra slot 0x80 structural receiver: %s (calls %s) ==\n' "$source" "$calls"
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportIndirectCallStructuralReceiverSlice.java "$entry" "$out" "$MAX_NODES" "$MAX_DEPTH"
done < <(tail -n +2 "$PLAN")

python3 "$REPO_ROOT/tools/ml/audit_nevergone_slot80_receiver_slices.py" \
  "$PLAN" "$FAILURES" "$INFERENCE" "$CATALOG" "$SLICE_DIR" \
  --details-output "$DETAILS" --require-complete | tee "$AUDIT_TXT"

printf '\nplan: %s\nslices: %s\ndetails: %s\naudit: %s\naffects routing: no\n' \
  "$PLAN" "$SLICE_DIR" "$DETAILS" "$AUDIT_TXT"
