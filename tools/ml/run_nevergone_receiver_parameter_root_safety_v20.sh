#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

IN="$WORK_ROOT/static-receiver-vtable-inference-v19.jsonl"
QIN="$WORK_ROOT/static-receiver-vtable-queries-v19.jsonl"
CAT="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RT="$WORK_ROOT/indirect-receiver-types.tsv"
SEED="$REPO_ROOT/tools/ml/nevergone_receiver_provenance_seed_plan.tsv"
TARGET="$WORK_ROOT/target-receiver-slot-provenance-v20.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v20.jsonl"
QOUT="$WORK_ROOT/static-receiver-vtable-queries-v20.jsonl"
DET="$WORK_ROOT/receiver-parameter-root-safety-v20.jsonl"
RES="$WORK_ROOT/static-receiver-vtable-resolution-v20.jsonl"
FAIL="$WORK_ROOT/receiver-type-failures-v20.jsonl"
AUD="$WORK_ROOT/receiver-type-failure-audit-v20.txt"
ARCH="$HOME/nevergone-receiver-parameter-root-safety-v20.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for f in "$IN" "$QIN" "$CAT" "$RT" "$SEED" \
  "$REPO_ROOT/tools/ghidra/ExportIndirectCallTargetReceiverProvenance.java" \
  "$REPO_ROOT/tools/ml/enforce_nevergone_receiver_parameter_root_safety_v20.py" \
  "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py"; do
  [[ -f "$f" ]] || { echo "missing: $f" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$WORK_ROOT"

echo "== regenerate canonical target receiver provenance =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallTargetReceiverProvenance.java "$SEED" "$TARGET"

echo
echo "== enforce receiver parameter-root identity =="
python3 "$REPO_ROOT/tools/ml/enforce_nevergone_receiver_parameter_root_safety_v20.py" \
  "$IN" "$QIN" "$TARGET" \
  --output "$OUT" --queries-output "$QOUT" --details-output "$DET" \
  --require-known-field-leaks

python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CAT" "$QOUT" --output "$RES"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RT" "$OUT" --details-output "$FAIL" --samples 80 | tee "$AUD"

python3 - "$IN" "$OUT" "$DET" "$QIN" "$QOUT" <<'PY'
import json,sys
known={("libcocos2dcpp.so:002a47c6",i) for i in range(6)}
def rd(p):
    with open(p,encoding='utf-8') as f:return [json.loads(x) for x in f if x.strip()]
def by(rows):return {(r.get('source_id'),r.get('call_index')):r for r in rows}
old=by(rd(sys.argv[1])); new=by(rd(sys.argv[2])); det=by(rd(sys.argv[3]))
if set(old)!=set(new): raise SystemExit('v20 inference identity set changed')
for k in known:
    if old[k].get('status') not in {'resolved_unique_vtable','resolved_unique_vtable_runtime_match'}:
        raise SystemExit(f'known field leak baseline not resolved: {k}')
    if old[k].get('class_evidence_source')!='parameter_caller_type':
        raise SystemExit(f'known field leak source changed: {k}')
    r=new[k]
    if r.get('status')!='unresolved_receiver_class' or r.get('receiver_vtable_id') is not None or r.get('class_name') is not None:
        raise SystemExit(f'known field leak was not demoted: {k}: {r}')
    d=det.get(k) or {}
    if d.get('accepted') is not False or 'receiver_not_parameter_root' not in d.get('reasons',[]):
        raise SystemExit(f'known field leak proof mismatch: {k}: {d}')
q0={(r.get('source_id'),r.get('call_index')) for r in rd(sys.argv[4])}; q1={(r.get('source_id'),r.get('call_index')) for r in rd(sys.argv[5])}
if not known.issubset(q0-q1): raise SystemExit(f'known field-leak queries not removed: {sorted(known-(q0-q1))}')
for k in {("libcocos2dcpp.so:002a47c6",6),("libcocos2dcpp.so:002a5dd0",0)}:
    if new[k].get('status')!='unresolved_receiver_class': raise SystemExit(f'v19 residual unexpectedly resolved: {k}')
remaining=sorted(k for k,r in new.items() if r.get('status')=='unresolved_receiver_class')
print('v20 parameter-root correction gate: pass')
print('remaining unresolved receiver classes:',len(remaining))
for k in remaining: print(' ',k)
PY

items=(
  static-receiver-vtable-inference-v20.jsonl
  static-receiver-vtable-queries-v20.jsonl
  static-receiver-vtable-resolution-v20.jsonl
  receiver-parameter-root-safety-v20.jsonl
  receiver-type-failures-v20.jsonl
  receiver-type-failure-audit-v20.txt
  target-receiver-slot-provenance-v20.tsv
  static-vtable-slot-catalog.jsonl
)
for x in "${items[@]}"; do [[ -f "$WORK_ROOT/$x" ]] || { echo "missing archive member: $x" >&2; exit 2; }; done
tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}"
printf '\narchive: %s\nknown BeatenEffect field leaks demoted: 6\naffects routing: no\n' "$ARCH"
