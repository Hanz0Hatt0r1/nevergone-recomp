#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${PLAN:=$REPO_ROOT/tools/ml/nevergone_receiver_provenance_seed_plan.tsv}"
: "${INFERENCE:=$WORK_ROOT/static-receiver-vtable-inference-v4.jsonl}"
: "${OUT:=$WORK_ROOT/target-receiver-slot-provenance.tsv}"
: "${DETAILS:=$WORK_ROOT/structural-slot-recovery-audit.jsonl}"
: "${AUDIT_TXT:=$WORK_ROOT/structural-slot-recovery-audit.txt}"
: "${SLICE_DIR:=$WORK_ROOT/evidence/indirect-slices}"
: "${SLICE_DETAILS:=$WORK_ROOT/missing-slot-target-slice-audit.jsonl}"
: "${SLICE_AUDIT_TXT:=$WORK_ROOT/missing-slot-target-slice-audit.txt}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$PLAN" ]] || { echo "missing seed plan: $PLAN" >&2; exit 2; }
[[ -f "$INFERENCE" ]] || {
  echo "missing persistent v4 inference: $INFERENCE" >&2
  echo "run tools/ml/run_nevergone_receiver_provenance_fresh.sh first" >&2
  exit 2
}
[[ -f "$REPO_ROOT/tools/ghidra/ExportIndirectCallTargetReceiverProvenance.java" ]] || {
  echo "missing target receiver provenance exporter" >&2
  exit 2
}
[[ -f "$REPO_ROOT/tools/ml/audit_nevergone_structural_slot_recovery.py" ]] || {
  echo "missing structural slot recovery audit" >&2
  exit 2
}
[[ -f "$REPO_ROOT/tools/ml/audit_nevergone_missing_slot_target_slices.py" ]] || {
  echo "missing missing-slot target slice audit" >&2
  exit 2
}
[[ -d "$SLICE_DIR" ]] || {
  echo "missing persistent CALLIND slices: $SLICE_DIR" >&2
  echo "run tools/ml/run_nevergone_receiver_provenance_fresh.sh first" >&2
  exit 2
}

mkdir -p "$WORK_ROOT"

printf 'plan: %s\n' "$PLAN"
printf 'v4 inference: %s\n' "$INFERENCE"
printf 'target provenance: %s\n' "$OUT"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallTargetReceiverProvenance.java "$PLAN" "$OUT"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_structural_slot_recovery.py" \
  "$INFERENCE" \
  "$OUT" \
  --details-output "$DETAILS" \
  --samples 20 \
  | tee "$AUDIT_TXT"

echo
echo "missing-slot target slice audit:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_missing_slot_target_slices.py" \
  "$INFERENCE" \
  "$SLICE_DIR" \
  --details-output "$SLICE_DETAILS" \
  | tee "$SLICE_AUDIT_TXT"

printf 'target provenance output: %s\n' "$OUT"
printf 'audit details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'slice audit details: %s\n' "$SLICE_DETAILS"
printf 'slice audit text: %s\n' "$SLICE_AUDIT_TXT"
printf 'affects routing: no\n'
