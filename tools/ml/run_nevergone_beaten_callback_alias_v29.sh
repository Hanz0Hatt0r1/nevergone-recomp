#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${SKIP_GHIDRA_EXPORT:=0}"

EVID="$WORK_ROOT/evidence/beaten-owner-call-v29"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
OUT="$WORK_ROOT/beaten-callback-alias-audit-v29.json"
ARCH="$HOME/nevergone-beaten-callback-alias-v29.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

[[ -f "$CATALOG" ]] || { echo "missing catalog: $CATALOG" >&2; exit 2; }
mkdir -p "$EVID"

if [[ "$SKIP_GHIDRA_EXPORT" != 1 ]]; then
  [[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
  for fn in 00525dac 00526c64 0051f3c8 0051eec0 0051ea1c; do
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
      -scriptPath "$REPO_ROOT/tools/ghidra" \
      -postScript ExportFunctionPcodeFlow.java "$fn" "$EVID/pcode-$fn.tsv"
  done
fi

for fn in 00525dac 00526c64 0051f3c8 0051eec0 0051ea1c; do
  [[ -f "$EVID/pcode-$fn.tsv" ]] || { echo "missing p-code: $fn" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_callback_alias_v29.py" \
  "$EVID" "$CATALOG" --output "$OUT"
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-callback-alias-audit-v29.json evidence/beaten-owner-call-v29
printf 'archive: %s\n' "$ARCH"
