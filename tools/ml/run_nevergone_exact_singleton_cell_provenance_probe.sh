#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${OWNER_AUDIT:=$WORK_ROOT/load-effective-address-owner-audit.jsonl}"
: "${CATALOG:=$WORK_ROOT/static-vtable-slot-catalog.jsonl}"
: "${ACTION_PROV:=$WORK_ROOT/actiondatamanager-singleton-cell-provenance.tsv}"
: "${ANDROID_PROV:=$WORK_ROOT/androidsdkmgr-singleton-cell-provenance.tsv}"
: "${DETAILS:=$WORK_ROOT/exact-singleton-cell-provenance-audit.jsonl}"
: "${AUDIT_TXT:=$WORK_ROOT/exact-singleton-cell-provenance-audit.txt}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
for required in \
  "$OWNER_AUDIT" "$CATALOG" \
  "$REPO_ROOT/tools/ghidra/ExportExactSingletonCellProvenance.java" \
  "$REPO_ROOT/tools/ml/audit_nevergone_exact_singleton_cell_provenance.py"; do
  [[ -e "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
mkdir -p "$WORK_ROOT"

# Fail closed before invoking Ghidra: the two exact cells and accessor owners must
# still match the independently captured owner/xref audit.
python3 - "$OWNER_AUDIT" <<'PY'
import json, sys
expected = {
    ("libcocos2dcpp.so:0029edb4", 0): ("0094cb44", "ActionDataManager::sharedActionDataManager"),
    ("libcocos2dcpp.so:002a08fc", 1): ("0094cbd8", "AndroidSdkMgr::getInstance"),
}
rows=[]
with open(sys.argv[1], encoding="utf-8") as f:
    for raw in f:
        if raw.strip(): rows.append(json.loads(raw))
for key, (cell, accessor) in expected.items():
    matches=[r for r in rows if (r.get("source_id"), r.get("call_index")) == key]
    if len(matches) != 1:
        raise SystemExit(f"owner audit row not unique for {key}: {len(matches)}")
    row=matches[0]
    got=str(row.get("effective_address") or "").lower().removeprefix("0x")
    if got != cell:
        raise SystemExit(f"owner cell mismatch for {key}: {got} != {cell}")
    if row.get("status") != "writable_effective_cell_with_refs":
        raise SystemExit(f"owner status mismatch for {key}: {row.get('status')}")
    owners=row.get("owner_functions")
    if not isinstance(owners, list) or accessor not in owners:
        raise SystemExit(f"accessor {accessor} is not an exact-cell xref owner for {key}: {owners}")
print("exact singleton owner gate: pass")
PY

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportExactSingletonCellProvenance.java \
  'ActionDataManager::sharedActionDataManager' 0094cb44 \
  'libcocos2dcpp.so:0029edb4' 0 "$ACTION_PROV"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportExactSingletonCellProvenance.java \
  'AndroidSdkMgr::getInstance' 0094cbd8 \
  'libcocos2dcpp.so:002a08fc' 1 "$ANDROID_PROV"

echo
echo "== exact singleton cell provenance audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_exact_singleton_cell_provenance.py" \
  "$OWNER_AUDIT" "$CATALOG" "$ACTION_PROV" "$ANDROID_PROV" \
  --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\naction provenance: %s\n' "$ACTION_PROV"
printf 'android provenance: %s\n' "$ANDROID_PROV"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
