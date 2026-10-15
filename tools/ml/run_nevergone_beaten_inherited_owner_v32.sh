#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${SKIP_GHIDRA_EXPORT:=0}"

EVID="$WORK_ROOT/evidence/beaten-inherited-owner-v32"
IMPL="$WORK_ROOT/evidence/beaten-owner-call-v29/pcode-00526c64.tsv"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
V29="$WORK_ROOT/beaten-callback-alias-audit-v29.json"
OUT="$WORK_ROOT/beaten-inherited-owner-audit-v32.json"
ARCH="$HOME/nevergone-beaten-inherited-owner-v32.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for input in "$IMPL" "$CATALOG" "$V29"; do
  [[ -f "$input" ]] || { echo "missing required input: $input" >&2; exit 2; }
done
mkdir -p "$EVID"
if [[ "$SKIP_GHIDRA_EXPORT" != 1 ]]; then
  [[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
  for fn in 00526b5c 00526c42 00525d4c; do
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
      -scriptPath "$REPO_ROOT/tools/ghidra" \
      -postScript ExportFunctionPcodeFlow.java "$fn" "$EVID/pcode-$fn.tsv"
  done
fi
for fn in 00526b5c 00526c42 00525d4c; do
  [[ -f "$EVID/pcode-$fn.tsv" ]] || { echo "missing p-code: $fn" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_inherited_owner_v32.py" \
  "$EVID" "$IMPL" "$CATALOG" "$V29" --output "$OUT"
python3 - "$CATALOG" "$EVID/ccsprite-slot-c8.jsonl" <<'PY'
import json
import sys
from pathlib import Path

source, output = map(Path, sys.argv[1:])
with source.open(encoding="utf-8") as stream:
    matches = []
    for line in stream:
        if not line.strip():
            continue
        row = json.loads(line)
        if row.get("class_name") == "cocos2d::CCSprite" and row.get("dispatch_slot_offset") == "0xc8":
            matches.append(line)
if len(matches) != 1:
    raise ValueError(f"expected one CCSprite slot 0xc8 row, got {len(matches)}")
output.write_text(matches[0], encoding="utf-8")
PY
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-inherited-owner-audit-v32.json beaten-callback-alias-audit-v29.json \
  evidence/beaten-inherited-owner-v32 \
  evidence/beaten-owner-call-v29/pcode-00526c64.tsv
printf 'archive: %s\n' "$ARCH"
