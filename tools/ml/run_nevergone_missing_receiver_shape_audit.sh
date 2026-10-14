#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v6.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
SEED_PLAN="$REPO_ROOT/tools/ml/nevergone_receiver_provenance_seed_plan.tsv"
STRUCTURAL="$WORK_ROOT/structural-receiver-parameter-provenance.tsv"
TARGET="$WORK_ROOT/target-receiver-slot-provenance.tsv"
DETAILS="$WORK_ROOT/missing-receiver-shape-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/missing-receiver-shape-audit.txt"

for required in "$INFERENCE" "$CATALOG" "$SEED_PLAN"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
for required in \
  tools/ghidra/ExportIndirectCallStructuralReceiverParameterProvenance.java \
  tools/ghidra/ExportIndirectCallTargetReceiverProvenance.java \
  tools/ml/audit_nevergone_missing_receiver_shapes.py; do
  [[ -f "$REPO_ROOT/$required" ]] || { echo "missing: $REPO_ROOT/$required" >&2; exit 2; }
done

mkdir -p "$WORK_ROOT"

echo "== structural receiver parameter provenance =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallStructuralReceiverParameterProvenance.java "$SEED_PLAN" "$STRUCTURAL"

echo
echo "== canonical target/slot provenance =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallTargetReceiverProvenance.java "$SEED_PLAN" "$TARGET"

echo
echo "== missing receiver shape audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_missing_receiver_shapes.py" \
  "$INFERENCE" "$STRUCTURAL" "$TARGET" "$CATALOG" \
  --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\ninference: %s\n' "$INFERENCE"
printf 'structural provenance: %s\n' "$STRUCTURAL"
printf 'target provenance: %s\n' "$TARGET"
printf 'audit details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
