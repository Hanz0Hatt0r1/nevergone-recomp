#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
CALLER_SUBTREES="$WORK_ROOT/caller-argument-subtrees.tsv"
ACCESSOR_AUDIT="$WORK_ROOT/gamesceneui-controls-accessor-audit.jsonl"
PLAN="$WORK_ROOT/gamesceneui-mixed-target-plan.tsv"
PROVENANCE="$WORK_ROOT/gamesceneui-mixed-target-provenance.tsv"
FACTORY_AUDIT="$WORK_ROOT/gamesceneui-mixed-target-factory-audit.jsonl"
FACTORY_AUDIT_TXT="$WORK_ROOT/gamesceneui-mixed-target-factory-audit.txt"
FIELD_STORES="$WORK_ROOT/gamesceneui-controls-field-stores.tsv"
DETAILS="$WORK_ROOT/gamesceneui-mixed-target-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/gamesceneui-mixed-target-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in "$CATALOG" "$CALLER_SUBTREES" "$ACCESSOR_AUDIT"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportFunctionReturnObjectProvenance.java" ]] || { echo "missing return provenance exporter" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportFieldStoreCandidates.java" ]] || { echo "missing field-store exporter" >&2; exit 2; }

# ELF symbol addresses are +0x10000 in the current Ghidra project view.
cat > "$PLAN" <<'EOF'
source_id	call_index	source_entry	call_address	dispatch_slot_offset	callee_entry	callee_name
probe:sharedGameSceneUI	0	003d95e0	-	0x80	003e95e0	sharedGameSceneUI
probe:initControls	0	00296a10	-	0x80	002a6a10	initControls
probe:initGameSceneUI	0	003d8fa8	-	0x80	003e8fa8	initGameSceneUI
EOF

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportFunctionReturnObjectProvenance.java "$PLAN" "$PROVENANCE"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportFieldStoreCandidates.java "GameSceneUI::initGameSceneUI" 0x134 "$FIELD_STORES"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_call_root_factory_returns.py" \
  "$PROVENANCE" "$CATALOG" --details-output "$FACTORY_AUDIT" \
  | tee "$FACTORY_AUDIT_TXT"

echo
printf '== GameSceneUI mixed receiver target convergence ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_gamesceneui_mixed_receiver_target.py" \
  "$FIELD_STORES" \
  "$PROVENANCE" \
  "$FACTORY_AUDIT" \
  "$ACCESSOR_AUDIT" \
  "$CALLER_SUBTREES" \
  "$CATALOG" \
  --details-output "$DETAILS" \
  | tee "$AUDIT_TXT"

printf '\nplan: %s\n' "$PLAN"
printf 'function provenance: %s\n' "$PROVENANCE"
printf 'factory audit: %s\n' "$FACTORY_AUDIT"
printf 'field stores: %s\n' "$FIELD_STORES"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
