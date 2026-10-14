#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

FAILURES="$WORK_ROOT/receiver-type-failures-v12.jsonl"
PROVENANCE="$WORK_ROOT/structural-receiver-parameter-provenance.tsv"
SLICE_DIR="$WORK_ROOT/evidence/structural-receiver-slices"
FUNCTION_CLASS="$WORK_ROOT/parameter-relative-function-classification.tsv"
SYMBOLS="$REPO_ROOT/docs/evidence/unidbg-2026-10-10/game.tsv"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
DETAILS="$WORK_ROOT/parameter-relative-receiver-field-audit.jsonl"
AUDIT="$WORK_ROOT/parameter-relative-receiver-field-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

if [[ ! -f "$FAILURES" ]]; then
  echo "v12 failure audit missing; regenerate current v12 baseline"
  bash "$REPO_ROOT/tools/ml/run_nevergone_exact_singleton_cell_recovery_v12.sh"
fi

for required in "$FAILURES" "$PROVENANCE" "$SYMBOLS" "$CATALOG" \
  "$REPO_ROOT/tools/ghidra/ExportAddressClassification.java"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -d "$SLICE_DIR" ]] || { echo "missing structural slice dir: $SLICE_DIR" >&2; exit 2; }
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java "$FUNCTION_CLASS" 002a647e

printf '\n== exact function identity ==\n'
cat "$FUNCTION_CLASS"
printf '\n== parameter-relative receiver field audit ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_parameter_relative_receiver_field.py" \
  "$FAILURES" "$PROVENANCE" "$SLICE_DIR" "$FUNCTION_CLASS" "$SYMBOLS" "$CATALOG" \
  --details-output "$DETAILS" | tee "$AUDIT"

printf '\nfunction classification: %s\n' "$FUNCTION_CLASS"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT"
printf 'affects routing: no\n'
