#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${REVENGINSTR_REPO:=$(cd -- "$REPO_ROOT/.." && pwd)/revenginstr}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v9.jsonl"
STRUCTURAL="$WORK_ROOT/structural-receiver-parameter-provenance.tsv"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
PLAN="$WORK_ROOT/caller-argument-subtree-plan.tsv"
SLICE="$WORK_ROOT/caller-argument-subtrees.tsv"
DETAILS="$WORK_ROOT/caller-argument-subtree-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/caller-argument-subtree-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in "$INFERENCE" "$STRUCTURAL" "$CATALOG"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportCallerArgumentSubtree.java" ]] || { echo "missing caller argument exporter" >&2; exit 2; }

CALLER_SCRIPT_PATH="$REPO_ROOT/tools/ghidra"
CALLER_SCRIPT_ARGS=("$PLAN" "$SLICE")
if [[ -f "$REVENGINSTR_REPO/ghidra/ExportGhidraCallerArgumentSubtree.java" ]]; then
  CALLER_SCRIPT_PATH="$REVENGINSTR_REPO/ghidra;$CALLER_SCRIPT_PATH"
  CALLER_SCRIPT_ARGS+=("$REVENGINSTR_REPO/ghidra")
fi

python3 "$REPO_ROOT/tools/ml/plan_nevergone_caller_argument_subtrees.py" "$INFERENCE" "$STRUCTURAL" "$PLAN"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$CALLER_SCRIPT_PATH" \
  -postScript ExportCallerArgumentSubtree.java "${CALLER_SCRIPT_ARGS[@]}"

echo
python3 "$REPO_ROOT/tools/ml/audit_nevergone_caller_argument_subtrees.py" \
  "$SLICE" "$CATALOG" --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\nplan: %s\n' "$PLAN"
printf 'subtrees: %s\n' "$SLICE"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
