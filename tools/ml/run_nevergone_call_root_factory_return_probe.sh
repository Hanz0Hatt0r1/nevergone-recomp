#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v8.jsonl"
SLICE_DIR="$WORK_ROOT/evidence/structural-receiver-slices"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
PLAN="$WORK_ROOT/call-root-factory-return-plan.tsv"
PROVENANCE="$WORK_ROOT/call-root-factory-return-provenance.tsv"
DETAILS="$WORK_ROOT/call-root-factory-return-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/call-root-factory-return-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

[[ -f "$INFERENCE" ]] || { echo "missing v8 inference: $INFERENCE" >&2; exit 2; }
[[ -d "$SLICE_DIR" ]] || { echo "missing structural slice dir: $SLICE_DIR" >&2; exit 2; }
[[ -f "$CATALOG" ]] || { echo "missing vtable catalog: $CATALOG" >&2; exit 2; }
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportFunctionReturnObjectProvenance.java" ]] || {
  echo "missing factory return exporter" >&2
  exit 2
}
mkdir -p "$WORK_ROOT"

python3 "$REPO_ROOT/tools/ml/plan_nevergone_call_root_factory_returns.py" \
  "$INFERENCE" "$SLICE_DIR" "$PLAN"

rows=$(( $(wc -l < "$PLAN") - 1 ))
if (( rows <= 0 )); then
  echo "no unresolved CALL-root receiver candidates" >&2
  exit 3
fi

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportFunctionReturnObjectProvenance.java "$PLAN" "$PROVENANCE"

echo
echo "== CALL-root factory return audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_call_root_factory_returns.py" \
  "$PROVENANCE" "$CATALOG" --details-output "$DETAILS" \
  | tee "$AUDIT_TXT"

printf '\nplan: %s\n' "$PLAN"
printf 'provenance: %s\n' "$PROVENANCE"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
