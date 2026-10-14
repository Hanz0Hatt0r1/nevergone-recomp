#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${ROOT_AUDIT:=$WORK_ROOT/nonload-structural-root-audit.jsonl}"
: "${CATALOG:=$WORK_ROOT/static-vtable-slot-catalog.jsonl}"
: "${SLICE_DIR:=$WORK_ROOT/evidence/structural-receiver-slices}"
: "${DETAILS:=$WORK_ROOT/structural-receiver-slice-audit.jsonl}"
: "${AUDIT_TXT:=$WORK_ROOT/structural-receiver-slice-audit.txt}"
: "${MAX_NODES:=96}"
: "${MAX_DEPTH:=16}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$ROOT_AUDIT" ]] || { echo "missing non-LOAD root audit: $ROOT_AUDIT" >&2; exit 2; }
[[ -f "$CATALOG" ]] || { echo "missing vtable catalog: $CATALOG" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportIndirectCallStructuralReceiverSlice.java" ]] || { echo "missing structural receiver slice exporter" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ml/audit_nevergone_structural_receiver_slices.py" ]] || { echo "missing structural receiver slice audit" >&2; exit 2; }

mkdir -p "$SLICE_DIR"

run_slice() {
  local source_id="$1"
  local entry="$2"
  local safe="${source_id//[:\/]/_}"
  local out="$SLICE_DIR/$safe.tsv"
  printf '\n== structural receiver slice: %s ==\n' "$source_id"
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" \
    -recursive \
    -readOnly \
    -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportIndirectCallStructuralReceiverSlice.java "$entry" "$out" "$MAX_NODES" "$MAX_DEPTH"
}

run_slice "libcocos2dcpp.so:002950ac" "002950ac"
run_slice "libcocos2dcpp.so:0029517c" "0029517c"
run_slice "libcocos2dcpp.so:002a647e" "002a647e"

printf '\n== structural receiver slice audit ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_structural_receiver_slices.py" \
  "$ROOT_AUDIT" \
  "$SLICE_DIR" \
  "$CATALOG" \
  --details-output "$DETAILS" \
  | tee "$AUDIT_TXT"

printf '\nslice dir: %s\n' "$SLICE_DIR"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
