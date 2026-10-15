#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${SKIP_GHIDRA_EXPORT:=0}"

EVID="$WORK_ROOT/evidence/beaten-child-dispatch-v35"
PRIOR="$WORK_ROOT/evidence/beaten-getparent-v34"
PARENT="$WORK_ROOT/evidence/beaten-inherited-owner-v32"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
V34="$WORK_ROOT/beaten-parent-dispatch-v34.json"
OUT="$WORK_ROOT/beaten-child-dispatch-v35.json"
ARCH="$HOME/nevergone-beaten-child-dispatch-v35.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for input in "$CATALOG" "$V34" "$PRIOR/pcode-00525e20.tsv" \
    "$PRIOR/pcode-00526bd4.tsv" "$PARENT/pcode-00525d4c.tsv"; do
  [[ -f "$input" ]] || { echo "missing required input: $input" >&2; exit 2; }
done
mkdir -p "$EVID"
if [[ "$SKIP_GHIDRA_EXPORT" != 1 ]]; then
  [[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
  for fn in 00525c7a 00525d8e 005270f8 00525fe0 00526fce; do
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
      -scriptPath "$REPO_ROOT/tools/ghidra" \
      -postScript ExportFunctionPcodeFlow.java "$fn" "$EVID/pcode-$fn.tsv"
  done
fi
for fn in 00525c7a 00525d8e 005270f8 00525fe0 00526fce; do
  [[ -f "$EVID/pcode-$fn.tsv" ]] || { echo "missing target p-code: $fn" >&2; exit 2; }
done
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_child_dispatch_v35.py" \
  "$PRIOR" "$EVID" "$PARENT" "$CATALOG" "$V34" --output "$OUT"
python3 - "$CATALOG" "$EVID/ccsprite-child-slots.jsonl" <<'PY'
import json
import sys
from pathlib import Path

source, output = map(Path, sys.argv[1:])
slots = {"0x2c", "0xa0", "0xc8", "0x138", "0x13c", "0x140"}
with source.open(encoding="utf-8") as stream:
    lines = []
    for line in stream:
        if not line.strip():
            continue
        row = json.loads(line)
        if row.get("class_name") == "cocos2d::CCSprite" and row.get("dispatch_slot_offset") in slots:
            lines.append(line)
if len(lines) != 6:
    raise ValueError(f"expected six CCSprite slot rows, got {len(lines)}")
output.write_text("".join(lines), encoding="utf-8")
PY
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-child-dispatch-v35.json beaten-parent-dispatch-v34.json \
  evidence/beaten-child-dispatch-v35 \
  evidence/beaten-getparent-v34/pcode-00525e20.tsv \
  evidence/beaten-getparent-v34/pcode-00526bd4.tsv \
  evidence/beaten-inherited-owner-v32/pcode-00525d4c.tsv
printf 'archive: %s\n' "$ARCH"
