#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${SKIP_GHIDRA_EXPORT:=0}"

EVID="$WORK_ROOT/evidence/beaten-child-lifecycle-v36"
PREVIOUS="$WORK_ROOT/beaten-child-dispatch-v35.json"
OUT="$WORK_ROOT/beaten-child-lifecycle-v36.json"
ARCH="$HOME/nevergone-beaten-child-lifecycle-v36.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

[[ -f "$PREVIOUS" ]] || { echo "missing v35 report: $PREVIOUS" >&2; exit 2; }
mkdir -p "$EVID"
if [[ "$SKIP_GHIDRA_EXPORT" != 1 ]]; then
  [[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
  for fn in 005270f8 00525fe0 00526fce; do
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
      -scriptPath "$REPO_ROOT/tools/ghidra" \
      -postScript ExportFunctionPcodeFlow.java "$fn" "$EVID/pcode-$fn.tsv"
  done
fi
for fn in 005270f8 00525fe0 00526fce; do
  [[ -f "$EVID/pcode-$fn.tsv" ]] || { echo "missing lifecycle p-code: $fn" >&2; exit 2; }
done
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_child_lifecycle_v36.py" \
  "$EVID" "$PREVIOUS" --output "$OUT"
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-child-lifecycle-v36.json beaten-child-dispatch-v35.json \
  evidence/beaten-child-lifecycle-v36
printf 'archive: %s\n' "$ARCH"
