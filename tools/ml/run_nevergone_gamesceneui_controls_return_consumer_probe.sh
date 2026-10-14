#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

ACCESSOR_TOKEN="${ACCESSOR_TOKEN:-GameSceneUI::GetControlsObj}"
ACCESSOR_AUDIT="$WORK_ROOT/gamesceneui-controls-accessor-audit.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
CONSUMERS="$WORK_ROOT/gamesceneui-controls-return-consumers.tsv"
DETAILS="$WORK_ROOT/gamesceneui-controls-return-consumer-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/gamesceneui-controls-return-consumer-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in "$ACCESSOR_AUDIT" "$CATALOG"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportAccessorReturnConsumers.java" ]] || { echo "missing accessor consumer exporter" >&2; exit 2; }

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAccessorReturnConsumers.java "$ACCESSOR_TOKEN" "$CONSUMERS"

python3 - "$CONSUMERS" <<'PY'
import csv, sys
path = sys.argv[1]
with open(path, encoding="utf-8", newline="") as f:
    rows = list(csv.DictReader(f, delimiter="\t"))
summaries = [r for r in rows if r.get("row_type") == "summary"]
if len(summaries) != 1:
    raise SystemExit(f"accessor consumer identity gate: expected 1 summary, got {len(summaries)}")
name = summaries[0].get("accessor_name") or ""
if name != "GameSceneUI::GetControlsObj":
    raise SystemExit(f"accessor consumer identity gate: expected GameSceneUI::GetControlsObj, got {name!r}")
print("accessor consumer identity gate: pass", name)
PY

echo
printf '== GameSceneUI controls return consumer audit ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_gamesceneui_controls_return_consumers.py" \
  "$CONSUMERS" "$ACCESSOR_AUDIT" "$CATALOG" \
  --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\nconsumer evidence: %s\n' "$CONSUMERS"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
