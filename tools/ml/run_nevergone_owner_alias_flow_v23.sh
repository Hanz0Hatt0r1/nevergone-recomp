#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

BW="$WORK_ROOT/residual-owner-field-writes-v21-beateneffect.tsv"
JW="$WORK_ROOT/residual-owner-field-writes-v21-ccjoystick.tsv"
PW="$WORK_ROOT/residual-owner-field-writes-v23-equiptoolslist.tsv"
BS="$WORK_ROOT/evidence/non-slot80-receiver-frontier-v18/libcocos2dcpp.so_002a47c6.tsv"
JS="$WORK_ROOT/evidence/residual-alias-fieldmap-v22/libcocos2dcpp.so_002a651c.tsv"
PS="$WORK_ROOT/evidence/residual-alias-fieldmap-v22/libcocos2dcpp.so_00299d90.tsv"
BF="$WORK_ROOT/owner-alias-flow-v23-beateneffect.tsv"
JF="$WORK_ROOT/owner-alias-flow-v23-ccjoystick.tsv"
PF="$WORK_ROOT/owner-alias-flow-v23-equiptoolslist.tsv"
FLOW_JSON="$WORK_ROOT/owner-alias-flow-v23-audit.jsonl"
FLOW_TXT="$WORK_ROOT/owner-alias-flow-v23-audit.txt"
CAND_JSON="$WORK_ROOT/residual-receiver-candidates-v23.jsonl"
CAND_TXT="$WORK_ROOT/residual-receiver-candidates-v23.txt"
ARCH="$HOME/nevergone-owner-alias-flow-v23.tar.gz"

for f in \
  "$BW" "$JW" "$BS" "$JS" "$PS" \
  "$REPO_ROOT/tools/ghidra/ExportClassOwnerAliasFlow.java" \
  "$REPO_ROOT/tools/ghidra/ExportClassParameterFieldWrites.java" \
  "$REPO_ROOT/tools/ml/audit_nevergone_owner_alias_flow_v23.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_residual_receiver_candidates_v23.py"; do
  [[ -f "$f" ]] || { echo "missing required input: $f" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$WORK_ROOT"

for spec in \
  "BeatenEffect:$BF" \
  "CCJoyStick:$JF" \
  "PVEDATA::equipToolsListMsg:$PF"; do
  cls="${spec%:*}"; out="${spec##*:}"
  echo "== owner alias flow: $cls =="
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportClassOwnerAliasFlow.java "$cls" "$out"
done

echo "== equipToolsListMsg exact parameter-field writes =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportClassParameterFieldWrites.java "PVEDATA::equipToolsListMsg" "$PW"

echo "== precise owner alias-flow audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_owner_alias_flow_v23.py" \
  --class-flow BeatenEffect "$BF" \
  --class-flow CCJoyStick "$JF" \
  --class-flow 'PVEDATA::equipToolsListMsg' "$PF" \
  --details-output "$FLOW_JSON" | tee "$FLOW_TXT"

echo "== residual receiver candidate join =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_residual_receiver_candidates_v23.py" \
  "$BW" "$JW" "$PW" "$BS" "$JS" "$PS" "$FLOW_JSON" \
  --details-output "$CAND_JSON" | tee "$CAND_TXT"

items=(
  residual-owner-field-writes-v21-beateneffect.tsv
  residual-owner-field-writes-v21-ccjoystick.tsv
  residual-owner-field-writes-v23-equiptoolslist.tsv
  owner-alias-flow-v23-beateneffect.tsv
  owner-alias-flow-v23-ccjoystick.tsv
  owner-alias-flow-v23-equiptoolslist.tsv
  owner-alias-flow-v23-audit.jsonl
  owner-alias-flow-v23-audit.txt
  residual-receiver-candidates-v23.jsonl
  residual-receiver-candidates-v23.txt
  static-receiver-vtable-inference-v20.jsonl
  receiver-type-failures-v20.jsonl
)
for x in "${items[@]}"; do [[ -f "$WORK_ROOT/$x" ]] || { echo "missing archive member: $WORK_ROOT/$x" >&2; exit 2; }; done

tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}" \
  evidence/non-slot80-receiver-frontier-v18/libcocos2dcpp.so_002a47c6.tsv \
  evidence/residual-alias-fieldmap-v22/libcocos2dcpp.so_002a651c.tsv \
  evidence/residual-alias-fieldmap-v22/libcocos2dcpp.so_00299d90.tsv

printf '\narchive: %s\nfield-value flows are not pointer-slot aliases\nreceiver class proven only when candidate audit says true\naffects routing: no\n' "$ARCH"
