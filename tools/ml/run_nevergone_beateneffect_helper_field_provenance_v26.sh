#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${REVENGINSTR_REPO:=$(cd -- "$REPO_ROOT/.." && pwd)/revenginstr}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
EVID="$WORK_ROOT/evidence/beateneffect-helper-provenance-v26"
HELPER_STORE="$EVID/helper-stores"
CALLEE_STORE="$EVID/whole-owner-callee-stores"
PLAN="$EVID/helper-caller-plan.tsv"
CALLERS="$EVID/helper-caller-arguments.tsv"
AUDIT_JSON="$WORK_ROOT/beateneffect-helper-field-provenance-v26.jsonl"
AUDIT_TXT="$WORK_ROOT/beateneffect-helper-field-provenance-v26.txt"
ARCH="$HOME/nevergone-beateneffect-helper-provenance-v26.tar.gz"
CAT="$WORK_ROOT/static-vtable-slot-catalog.jsonl"

mkdir -p "$HELPER_STORE" "$CALLEE_STORE"
for f in \
  "$REPO_ROOT/tools/ml/run_nevergone_protobuf_nested_receiver_recovery_v25.sh" \
  "$REPO_ROOT/tools/ml/plan_nevergone_beateneffect_helper_callers_v26.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_beateneffect_helper_field_provenance_v26.py" \
  "$REPO_ROOT/tools/ghidra/ExportExactParameterFieldFactoryStore.java" \
  "$REPO_ROOT/tools/ghidra/ExportCallerArgumentSubtree.java"; do
  [[ -f "$f" ]] || { echo "missing required tool: $f" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }

archive_v26() {
  local items=()
  for f in beateneffect-helper-field-provenance-v26.jsonl beateneffect-helper-field-provenance-v26.txt static-vtable-slot-catalog.jsonl; do
    [[ -f "$WORK_ROOT/$f" ]] && items+=("$f")
  done
  for f in static-receiver-vtable-inference-v25.jsonl static-receiver-vtable-queries-v25.jsonl receiver-type-failure-audit-v25.txt; do
    [[ -f "$WORK_ROOT/$f" ]] && items+=("$f")
  done
  [[ -d "$EVID" ]] && items+=("evidence/beateneffect-helper-provenance-v26")
  [[ -d "$WORK_ROOT/evidence/protobuf-nested-recovery-v25" ]] && items+=("evidence/protobuf-nested-recovery-v25")
  if ((${#items[@]})); then tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}"; fi
}
trap 'rc=$?; archive_v26 || true; exit $rc' EXIT

if [[ ! -f "$WORK_ROOT/static-receiver-vtable-inference-v25.jsonl" || ! -f "$CAT" ]]; then
  echo "== v25 prerequisite missing: run v25 first =="
  bash "$REPO_ROOT/tools/ml/run_nevergone_protobuf_nested_receiver_recovery_v25.sh"
fi

echo "== exact helper parameter-relative factory stores =="
for fn in 002a3a20 002a3c74; do
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportExactParameterFieldFactoryStore.java "$fn" "$HELPER_STORE/store-$fn.tsv"
done

python3 "$REPO_ROOT/tools/ml/plan_nevergone_beateneffect_helper_callers_v26.py" \
  --helper 002a3a20 "$HELPER_STORE/store-002a3a20.tsv" \
  --helper 002a3c74 "$HELPER_STORE/store-002a3c74.tsv" \
  --output "$PLAN"

echo "== all direct caller argument subtrees for helper destination parameters =="
CALLER_SCRIPT_PATH="$REPO_ROOT/tools/ghidra"
CALLER_SCRIPT_ARGS=("$PLAN" "$CALLERS")
if [[ -f "$REVENGINSTR_REPO/ghidra/ExportGhidraCallerArgumentSubtree.java" ]]; then
  CALLER_SCRIPT_PATH="$REVENGINSTR_REPO/ghidra;$CALLER_SCRIPT_PATH"
  CALLER_SCRIPT_ARGS+=("$REVENGINSTR_REPO/ghidra")
fi
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$CALLER_SCRIPT_PATH" \
  -postScript ExportCallerArgumentSubtree.java "${CALLER_SCRIPT_ARGS[@]}"

echo "== exact stores through known whole-owner callees =="
for fn in 00526124 005353f8 00535204 00525dac 0051f3c8; do
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportExactParameterFieldFactoryStore.java "$fn" "$CALLEE_STORE/store-$fn.tsv"
done

echo "== v26 BeatenEffect helper/alias evidence audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beateneffect_helper_field_provenance_v26.py" \
  --helper-store 002a3a20 "$HELPER_STORE/store-002a3a20.tsv" \
  --helper-store 002a3c74 "$HELPER_STORE/store-002a3c74.tsv" \
  --callers "$CALLERS" \
  --owner-callee-store 00526124 "$CALLEE_STORE/store-00526124.tsv" \
  --owner-callee-store 005353f8 "$CALLEE_STORE/store-005353f8.tsv" \
  --owner-callee-store 00535204 "$CALLEE_STORE/store-00535204.tsv" \
  --owner-callee-store 00525dac "$CALLEE_STORE/store-00525dac.tsv" \
  --owner-callee-store 0051f3c8 "$CALLEE_STORE/store-0051f3c8.tsv" \
  --catalog "$CAT" --details-output "$AUDIT_JSON" | tee "$AUDIT_TXT"

archive_v26
trap - EXIT
printf '\narchive: %s\nfrontier input after v25: 10 unresolved\nhelper destinations traced: 002a3a20, 002a3c74\nknown whole-owner callees inspected: 5\naffects routing: no\n' "$ARCH"
