#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v5.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
PLAN="$WORK_ROOT/resolved-caller-verification-plan.tsv"
COMPLETENESS="$WORK_ROOT/resolved-caller-type-completeness.tsv"
TARGET_PROVENANCE="$WORK_ROOT/target-receiver-slot-provenance.tsv"
DETAILS="$WORK_ROOT/resolved-caller-safety-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/resolved-caller-safety-audit.txt"
SEED_PLAN="$REPO_ROOT/tools/ml/nevergone_receiver_provenance_seed_plan.tsv"

for required in "$INFERENCE" "$CATALOG" "$SEED_PLAN"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
for required in \
  tools/ml/build_nevergone_resolved_caller_verification_plan.py \
  tools/ml/audit_nevergone_resolved_caller_safety.py \
  tools/ghidra/ExportResolvedCallerTypeCompleteness.java \
  tools/ghidra/ExportIndirectCallTargetReceiverProvenance.java; do
  [[ -f "$REPO_ROOT/$required" ]] || { echo "missing: $REPO_ROOT/$required" >&2; exit 2; }
done

mkdir -p "$WORK_ROOT"

python3 "$REPO_ROOT/tools/ml/build_nevergone_resolved_caller_verification_plan.py" \
  "$INFERENCE" --output "$PLAN"

echo
echo "== strict direct-caller completeness =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportResolvedCallerTypeCompleteness.java "$PLAN" "$COMPLETENESS"

echo
echo "== canonical target/slot provenance =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallTargetReceiverProvenance.java "$SEED_PLAN" "$TARGET_PROVENANCE"

echo
echo "== resolved caller safety gate =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_resolved_caller_safety.py" \
  "$INFERENCE" "$COMPLETENESS" "$TARGET_PROVENANCE" "$CATALOG" \
  --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\nplan: %s\n' "$PLAN"
printf 'caller completeness: %s\n' "$COMPLETENESS"
printf 'target provenance: %s\n' "$TARGET_PROVENANCE"
printf 'audit details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
