#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${SKIP_GHIDRA_EXPORT:=0}"
source "$REPO_ROOT/tools/ghidra/revenginstr_pcode_flow_script_path.sh"
PCODE_SCRIPT_PATH=$(revenginstr_pcode_flow_script_path "$REPO_ROOT")

EVID="$WORK_ROOT/evidence/beaten-getparent-v34"
PRIOR="$WORK_ROOT/evidence/beaten-child-parent-v33"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
V33="$WORK_ROOT/beaten-child-parent-reads-v33.json"
OUT="$WORK_ROOT/beaten-parent-dispatch-v34.json"
ARCH="$HOME/nevergone-beaten-parent-dispatch-v34.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for input in "$CATALOG" "$V33"; do
  [[ -f "$input" ]] || { echo "missing required input: $input" >&2; exit 2; }
done
for fn in 00525c80 00525dde 0054934a; do
  [[ -f "$PRIOR/pcode-$fn.tsv" ]] || { echo "missing v33 p-code: $fn" >&2; exit 2; }
done
mkdir -p "$EVID"
if [[ "$SKIP_GHIDRA_EXPORT" != 1 ]]; then
  [[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportAccessorReturnConsumers.java 00525d46 "$EVID/getparent-consumers.tsv"
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportAddressReferenceOwners.java "$EVID/getparent-references.tsv" 00525d46
  for fn in 00526c1e 00525e20 00526bd4; do
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
      -scriptPath "$PCODE_SCRIPT_PATH" \
      -postScript ExportFunctionPcodeFlow.java "$fn" "$EVID/pcode-$fn.tsv"
  done
fi
for input in getparent-consumers.tsv getparent-references.tsv \
    pcode-00526c1e.tsv pcode-00525e20.tsv pcode-00526bd4.tsv; do
  [[ -f "$EVID/$input" ]] || { echo "missing v34 evidence: $input" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_parent_dispatch_v34.py" \
  "$EVID" "$PRIOR" "$CATALOG" "$V33" --output "$OUT"
python3 - "$CATALOG" "$EVID/beaten-parent-slots.jsonl" <<'PY'
import json
import sys
from pathlib import Path

source, output = map(Path, sys.argv[1:])
slots = {"0xcc", "0xdc", "0xf0"}
with source.open(encoding="utf-8") as stream:
    lines = []
    for line in stream:
        if not line.strip():
            continue
        row = json.loads(line)
        if row.get("class_name") == "BeatenEffect" and row.get("dispatch_slot_offset") in slots:
            lines.append(line)
if len(lines) != 3:
    raise ValueError(f"expected three BeatenEffect slot rows, got {len(lines)}")
output.write_text("".join(lines), encoding="utf-8")
PY
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-parent-dispatch-v34.json beaten-child-parent-reads-v33.json \
  evidence/beaten-getparent-v34 \
  evidence/beaten-child-parent-v33/pcode-00525c80.tsv \
  evidence/beaten-child-parent-v33/pcode-00525dde.tsv \
  evidence/beaten-child-parent-v33/pcode-0054934a.tsv
printf 'archive: %s\n' "$ARCH"
