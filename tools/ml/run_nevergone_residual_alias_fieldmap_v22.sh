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
BS="$WORK_ROOT/evidence/non-slot80-receiver-frontier-v18/libcocos2dcpp.so_002a47c6.tsv"
BA="$WORK_ROOT/residual-owner-alias-v22-beateneffect.tsv"
JA="$WORK_ROOT/residual-owner-alias-v22-ccjoystick.tsv"
EVID="$WORK_ROOT/evidence/residual-alias-fieldmap-v22"
S299="$EVID/libcocos2dcpp.so_00299d90.tsv"
S651="$EVID/libcocos2dcpp.so_002a651c.tsv"
IDS="$EVID/function-identities.tsv"
ALIAS_JSON="$WORK_ROOT/residual-owner-alias-frontier-v22.jsonl"
ALIAS_TXT="$WORK_ROOT/residual-owner-alias-frontier-v22.txt"
SHAPE_JSON="$WORK_ROOT/newly-unresolved-field-shapes-v22.jsonl"
SHAPE_TXT="$WORK_ROOT/newly-unresolved-field-shapes-v22.txt"
ARCH="$HOME/nevergone-residual-alias-fieldmap-v22.tar.gz"

for f in \
  "$BW" "$JW" "$BS" \
  "$WORK_ROOT/static-receiver-vtable-inference-v20.jsonl" \
  "$WORK_ROOT/receiver-type-failures-v20.jsonl" \
  "$REPO_ROOT/tools/ghidra/ExportClassOwnerAliasInventory.java" \
  "$REPO_ROOT/tools/ghidra/ExportIndirectCallStructuralReceiverSlice.java" \
  "$REPO_ROOT/tools/ghidra/ExportAddressClassification.java" \
  "$REPO_ROOT/tools/ml/audit_nevergone_residual_owner_alias_frontier_v22.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_newly_unresolved_field_shapes_v22.py"; do
  [[ -f "$f" ]] || { echo "missing required input: $f" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$EVID"

for spec in "BeatenEffect:$BA" "CCJoyStick:$JA"; do
  cls="${spec%%:*}"; out="${spec#*:}"
  echo "== class-wide alias inventory: $cls =="
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportClassOwnerAliasInventory.java "$cls" "$out"
done

for spec in "00299d90:$S299" "002a651c:$S651"; do
  entry="${spec%%:*}"; out="${spec#*:}"
  echo "== structural receiver slice: $entry =="
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportIndirectCallStructuralReceiverSlice.java "$entry" "$out" 1024 96
done

echo "== exact residual function identities =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java "$IDS" 00299d90 002a651c

echo "== owner alias frontier audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_residual_owner_alias_frontier_v22.py" \
  "$BW" "$JW" "$BA" "$JA" "$BS" --details-output "$ALIAS_JSON" | tee "$ALIAS_TXT"

echo "== newly unresolved structural field-shape audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_newly_unresolved_field_shapes_v22.py" \
  "$S299" "$S651" "$IDS" --details-output "$SHAPE_JSON" | tee "$SHAPE_TXT"

items=(
  residual-owner-field-writes-v21-beateneffect.tsv
  residual-owner-field-writes-v21-ccjoystick.tsv
  residual-owner-alias-v22-beateneffect.tsv
  residual-owner-alias-v22-ccjoystick.tsv
  residual-owner-alias-frontier-v22.jsonl
  residual-owner-alias-frontier-v22.txt
  newly-unresolved-field-shapes-v22.jsonl
  newly-unresolved-field-shapes-v22.txt
  static-receiver-vtable-inference-v20.jsonl
  receiver-type-failures-v20.jsonl
)
for x in "${items[@]}"; do [[ -f "$WORK_ROOT/$x" ]] || { echo "missing archive member: $WORK_ROOT/$x" >&2; exit 2; }; done

tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}" \
  evidence/non-slot80-receiver-frontier-v18/libcocos2dcpp.so_002a47c6.tsv \
  evidence/residual-alias-fieldmap-v22
printf '\narchive: %s\nreceiver class proven only when audit says true\naffects routing: no\n' "$ARCH"
