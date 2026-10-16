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
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
EVID="$WORK_ROOT/evidence/residual-closure-v24"
PCODE="$EVID/pcode"
SLICE="$EVID/slices"
PROTO_FLOW="$WORK_ROOT/owner-alias-flow-v24-equiptoolslist.tsv"
AUDIT_JSON="$WORK_ROOT/owner-alias-flow-v24-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/owner-alias-flow-v24-audit.txt"
DETAIL_JSON="$WORK_ROOT/residual-receiver-closure-v24.jsonl"
DETAIL_TXT="$WORK_ROOT/residual-receiver-closure-v24.txt"
ARCH="$HOME/nevergone-residual-receiver-closure-v24.tar.gz"
BEAT_FLOW="$WORK_ROOT/owner-alias-flow-v23-beateneffect.tsv"
JOY_FLOW="$WORK_ROOT/owner-alias-flow-v23-ccjoystick.tsv"
PROTO_WRITES="$WORK_ROOT/residual-owner-field-writes-v23-equiptoolslist.tsv"

mkdir -p "$PCODE" "$SLICE"
for f in \
  "$BEAT_FLOW" "$JOY_FLOW" "$PROTO_WRITES" \
  "$REPO_ROOT/tools/ghidra/ExportClassOwnerAliasFlow.java" \
  "$REPO_ROOT/tools/ghidra/ExportFunctionPcodeFlow.java" \
  "$REPO_ROOT/tools/ghidra/ExportIndirectCallStructuralReceiverSlice.java" \
  "$REPO_ROOT/tools/ml/audit_nevergone_owner_alias_flow_v23.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_residual_closure_v24.py"; do
  [[ -f "$f" ]] || { echo "missing required input: $f" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }

archive_v24() {
  local items=()
  for f in \
    owner-alias-flow-v23-beateneffect.tsv \
    owner-alias-flow-v23-ccjoystick.tsv \
    residual-owner-field-writes-v23-equiptoolslist.tsv \
    owner-alias-flow-v24-equiptoolslist.tsv \
    owner-alias-flow-v24-audit.jsonl \
    owner-alias-flow-v24-audit.txt \
    residual-receiver-closure-v24.jsonl \
    residual-receiver-closure-v24.txt; do
    [[ -f "$WORK_ROOT/$f" ]] && items+=("$f")
  done
  [[ -d "$EVID" ]] && items+=("evidence/residual-closure-v24")
  if ((${#items[@]})); then tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}"; fi
}
trap 'rc=$?; archive_v24 || true; exit $rc' EXIT

echo "== corrected protobuf owner alias flow =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportClassOwnerAliasFlow.java 'PVEDATA::equipToolsListMsg' "$PROTO_FLOW"

echo "== corrected three-class alias audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_owner_alias_flow_v23.py" \
  --class-flow BeatenEffect "$BEAT_FLOW" \
  --class-flow CCJoyStick "$JOY_FLOW" \
  --class-flow 'PVEDATA::equipToolsListMsg' "$PROTO_FLOW" \
  --details-output "$AUDIT_JSON" | tee "$AUDIT_TXT"

pcode_functions=(
  00299a66 00299a88 00299aaa 00299acc 00299fae 0029c20c
  002a3908 002a3e94 002a474c 002a47c6
  002a5f00 002a6284 002a6584
)
for fn in "${pcode_functions[@]}"; do
  echo "== pcode $fn =="
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$PCODE_SCRIPT_PATH" \
    -postScript ExportFunctionPcodeFlow.java "$fn" "$PCODE/pcode-$fn.tsv"
done

slice_functions=(002a3908 002a3e94 002a474c 002a47c6 002a5f00 002a6284 002a6584)
for fn in "${slice_functions[@]}"; do
  echo "== callind structural slice $fn =="
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportIndirectCallStructuralReceiverSlice.java "$fn" "$SLICE/slice-$fn.tsv" 200 32
done

echo "== v24 targeted closure audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_residual_closure_v24.py" \
  --flow BeatenEffect "$BEAT_FLOW" \
  --flow CCJoyStick "$JOY_FLOW" \
  --flow 'PVEDATA::equipToolsListMsg' "$PROTO_FLOW" \
  --proto-writes "$PROTO_WRITES" \
  --pcode-dir "$PCODE" --slice-dir "$SLICE" \
  --details-output "$DETAIL_JSON" | tee "$DETAIL_TXT"

archive_v24
trap - EXIT
printf '\narchive: %s\nprotobuf namespace owner detection corrected\ntargeted p-code functions: %s\nCALLIND slice functions: %s\naffects routing: no\n' "$ARCH" "${#pcode_functions[@]}" "${#slice_functions[@]}"
