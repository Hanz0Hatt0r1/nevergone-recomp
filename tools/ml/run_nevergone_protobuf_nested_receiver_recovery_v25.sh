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
IN="$WORK_ROOT/static-receiver-vtable-inference-v20.jsonl"
QIN="$WORK_ROOT/static-receiver-vtable-queries-v20.jsonl"
CAT="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RT="$WORK_ROOT/indirect-receiver-types.tsv"
PROTO_SLICE="$WORK_ROOT/evidence/residual-alias-fieldmap-v22/libcocos2dcpp.so_00299d90.tsv"
PROTO_WRITES="$WORK_ROOT/residual-owner-field-writes-v23-equiptoolslist.tsv"
PROTO_FLOW="$WORK_ROOT/owner-alias-flow-v24-equiptoolslist.tsv"
PCODE24="$WORK_ROOT/evidence/residual-closure-v24/pcode"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v25.jsonl"
QOUT="$WORK_ROOT/static-receiver-vtable-queries-v25.jsonl"
DET="$WORK_ROOT/protobuf-nested-receiver-recovery-v25.jsonl"
RES="$WORK_ROOT/static-receiver-vtable-resolution-v25.jsonl"
FAIL="$WORK_ROOT/receiver-type-failures-v25.jsonl"
AUD="$WORK_ROOT/receiver-type-failure-audit-v25.txt"
EVID25="$WORK_ROOT/evidence/protobuf-nested-recovery-v25"
ARCH="$HOME/nevergone-protobuf-nested-recovery-v25.tar.gz"

mkdir -p "$EVID25"
for f in \
  "$IN" "$QIN" "$CAT" "$RT" "$PROTO_SLICE" "$PROTO_WRITES" "$PROTO_FLOW" \
  "$PCODE24/pcode-00299a66.tsv" "$PCODE24/pcode-00299a88.tsv" \
  "$PCODE24/pcode-00299aaa.tsv" "$PCODE24/pcode-00299acc.tsv" \
  "$PCODE24/pcode-00299fae.tsv" \
  "$REPO_ROOT/tools/ml/finalize_nevergone_protobuf_nested_receiver_recovery_v25.py" \
  "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$REPO_ROOT/tools/ghidra/ExportFunctionPcodeFlow.java"; do
  [[ -f "$f" ]] || { echo "missing required input: $f" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }

archive_v25() {
  local items=()
  for f in \
    static-receiver-vtable-inference-v25.jsonl \
    static-receiver-vtable-queries-v25.jsonl \
    static-receiver-vtable-resolution-v25.jsonl \
    protobuf-nested-receiver-recovery-v25.jsonl \
    receiver-type-failures-v25.jsonl \
    receiver-type-failure-audit-v25.txt \
    static-vtable-slot-catalog.jsonl; do
    [[ -f "$WORK_ROOT/$f" ]] && items+=("$f")
  done
  [[ -d "$EVID25" ]] && items+=("evidence/protobuf-nested-recovery-v25")
  if ((${#items[@]})); then tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}"; fi
}
trap 'rc=$?; archive_v25 || true; exit $rc' EXIT

echo "== finalize exact protobuf nested receiver lifecycle =="
python3 "$REPO_ROOT/tools/ml/finalize_nevergone_protobuf_nested_receiver_recovery_v25.py" \
  "$IN" "$QIN" "$PROTO_SLICE" "$PROTO_WRITES" "$PROTO_FLOW" "$PCODE24" "$CAT" \
  --output "$OUT" --queries-output "$QOUT" --details-output "$DET" --require-four

echo "== resolve v25 receiver queries =="
python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CAT" "$QOUT" --output "$RES"

echo "== audit v25 receiver failures =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RT" "$OUT" --details-output "$FAIL" --samples 80 | tee "$AUD"

python3 - "$IN" "$OUT" "$DET" <<'PY'
import json,sys
SOURCE='libcocos2dcpp.so:00299d90'
PROTO={(SOURCE,i) for i in range(4)}
EXPECTED_REMAINING={("libcocos2dcpp.so:002a47c6",i) for i in range(7)} | {
    ("libcocos2dcpp.so:002a5dd0",0),
    ("libcocos2dcpp.so:002a651c",0),
    ("libcocos2dcpp.so:002a651c",1),
}
def rd(p):
    with open(p,encoding='utf-8') as f:return [json.loads(x) for x in f if x.strip()]
def by(rows):return {(r.get('source_id'),r.get('call_index')):r for r in rows}
old=by(rd(sys.argv[1]));new=by(rd(sys.argv[2]));det=by(rd(sys.argv[3]))
if set(old)!=set(new):raise SystemExit('v25 inference identity set changed')
changed={k for k in old if old[k]!=new[k]}
if changed!=PROTO:raise SystemExit(f'v25 changed unexpected rows: {sorted(changed)}')
for k in sorted(PROTO):
    r=new[k];d=det.get(k) or {}
    if old[k].get('status')!='unresolved_receiver_class':raise SystemExit(f'protobuf baseline not unresolved: {k}')
    if r.get('status')!='resolved_unique_vtable':raise SystemExit(f'protobuf row not resolved: {k}: {r.get("status")}')
    if r.get('class_name')!='PVEDATA::equipToolsDataMsg':raise SystemExit(f'protobuf class mismatch: {k}')
    if r.get('receiver_vtable_id')!='libcocos2dcpp.so:008f32b0':raise SystemExit(f'protobuf vtable mismatch: {k}')
    if d.get('accepted') is not True or d.get('candidate_target_ids')!=['libcocos2dcpp.so:00299b1c']:
        raise SystemExit(f'protobuf proof mismatch: {k}: {d}')
remaining={k for k,r in new.items() if r.get('status')=='unresolved_receiver_class'}
if remaining!=EXPECTED_REMAINING:raise SystemExit(f'unexpected v25 unresolved frontier: {sorted(remaining)}')
print('v25 protobuf nested receiver gate: pass')
print('recovered protobuf receiver classes: 4')
print('remaining unresolved receiver classes:',len(remaining))
for k in sorted(remaining):print(' ',k)
PY

echo "== targeted BeatenEffect remaining-field p-code =="
for fn in 002a3a20 002a3c74; do
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$PCODE_SCRIPT_PATH" \
    -postScript ExportFunctionPcodeFlow.java "$fn" "$EVID25/pcode-$fn.tsv"
done

archive_v25
trap - EXIT
printf '\narchive: %s\nprotobuf nested receivers recovered: 4\nremaining unresolved receiver classes: 10\nBeatenEffect targeted p-code: 002a3a20, 002a3c74\naffects routing: no\n' "$ARCH"
