#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

# game.tsv records the ELF symbol at 0x003d6eee. The analyzed Ghidra project
# uses the +0x10000 image-base view for this binary, so GetControlsObj is at
# 0x003e6eee. The exported identity is checked independently below.
ACCESSOR_ENTRY="${ACCESSOR_ENTRY:-003e6eee}"
CALLER_SUBTREES="$WORK_ROOT/caller-argument-subtrees.tsv"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
ACCESSOR_TSV="$WORK_ROOT/gamesceneui-controls-accessor.tsv"
DETAILS="$WORK_ROOT/gamesceneui-controls-accessor-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/gamesceneui-controls-accessor-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in "$CALLER_SUBTREES" "$CATALOG"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportAccessorReturnField.java" ]] || { echo "missing accessor exporter" >&2; exit 2; }

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAccessorReturnField.java "$ACCESSOR_ENTRY" "$ACCESSOR_TSV"

python3 - "$ACCESSOR_TSV" <<'PY'
import csv
import sys
from pathlib import Path

path = Path(sys.argv[1])
with path.open("r", encoding="utf-8", newline="") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))
if not rows:
    raise SystemExit(f"accessor identity gate: no rows in {path}")
names = {row.get("function_name", "") for row in rows}
entries = {row.get("function_entry", "") for row in rows}
if names != {"GetControlsObj"}:
    raise SystemExit(
        "accessor identity gate: expected GetControlsObj, got "
        + repr(sorted(names)) + " at " + repr(sorted(entries))
    )
print("accessor identity gate: pass", sorted(entries), sorted(names))
PY

echo
printf '== GameSceneUI controls accessor audit ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_gamesceneui_controls_accessor.py" \
  "$ACCESSOR_TSV" "$CALLER_SUBTREES" "$CATALOG" \
  --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\naccessor evidence: %s\n' "$ACCESSOR_TSV"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
