#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
source "$REPO_ROOT/tools/ghidra/revenginstr_pcode_flow_script_path.sh"
PCODE_SCRIPT_PATH=$(revenginstr_pcode_flow_script_path "$REPO_ROOT")
: "${SKIP_GHIDRA_EXPORT:=0}"

EVID="$WORK_ROOT/evidence/beaten-child-parent-v33"
PREVIOUS="$WORK_ROOT/beaten-inherited-owner-audit-v32.json"
OUT="$WORK_ROOT/beaten-child-parent-reads-v33.json"
ARCH="$HOME/nevergone-beaten-child-parent-reads-v33.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

[[ -f "$PREVIOUS" ]] || { echo "missing v32 report: $PREVIOUS" >&2; exit 2; }
mkdir -p "$EVID"
if [[ "$SKIP_GHIDRA_EXPORT" != 1 ]]; then
  [[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportClassFieldLoadCandidates.java \
    'cocos2d::CCNode::;cocos2d::CCSprite::' 0xa8 "$EVID/parent-loads.tsv"
  for fn in 00525c80 00525d46 00525dde 00526e0c 0054934a 005499b4; do
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
      -scriptPath "$PCODE_SCRIPT_PATH" \
      -postScript ExportFunctionPcodeFlow.java "$fn" "$EVID/pcode-$fn.tsv"
  done
fi
[[ -f "$EVID/parent-loads.tsv" ]] || { echo "missing parent load inventory" >&2; exit 2; }
for fn in 00525c80 00525d46 00525dde 00526e0c 0054934a 005499b4; do
  [[ -f "$EVID/pcode-$fn.tsv" ]] || { echo "missing p-code: $fn" >&2; exit 2; }
done
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_child_parent_reads_v33.py" \
  "$EVID/parent-loads.tsv" "$EVID" "$PREVIOUS" --output "$OUT"
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-child-parent-reads-v33.json beaten-inherited-owner-audit-v32.json \
  evidence/beaten-child-parent-v33
printf 'archive: %s\n' "$ARCH"
