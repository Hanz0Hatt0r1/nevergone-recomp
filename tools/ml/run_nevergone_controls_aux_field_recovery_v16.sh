#!/usr/bin/env bash
set -euo pipefail
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
if [[ $# -ne 3 ]]; then echo "Usage: bash $0 APK field-alias-evidence.tar.gz operand-roots.tar.gz" >&2; exit 2; fi
APK="$1"; FIELD="$2"; OPERAND="$3"
IN15="$WORK_ROOT/static-receiver-vtable-inference-v15.jsonl"; Q15="$WORK_ROOT/static-receiver-vtable-queries-v15.jsonl"
GATED="$WORK_ROOT/controlsbutton-packaged-field-stability.jsonl"; FACTORY="$WORK_ROOT/slot80-factory-field-candidates-v15.jsonl"; CAT="$WORK_ROOT/static-vtable-slot-catalog.jsonl"; RT="$WORK_ROOT/indirect-receiver-types.tsv"
ST="$WORK_ROOT/controls-aux-field-stability-v16.jsonl"; OUT="$WORK_ROOT/static-receiver-vtable-inference-v16.jsonl"; OQ="$WORK_ROOT/static-receiver-vtable-queries-v16.jsonl"; DET="$WORK_ROOT/controls-aux-field-recovery-v16.jsonl"; RES="$WORK_ROOT/static-receiver-vtable-resolution-v16.jsonl"; FAIL="$WORK_ROOT/receiver-type-failures-v16.jsonl"; AUD="$WORK_ROOT/receiver-type-failure-audit-v16.txt"; ARCH="$HOME/nevergone-controls-aux-field-recovery-v16.tar.gz"
for f in "$APK" "$FIELD" "$OPERAND" "$REPO_ROOT/tools/ml/run_nevergone_controlsbutton_slot80_field_recovery_v15.sh" "$REPO_ROOT/tools/ml/audit_nevergone_controls_aux_field_stability.py" "$REPO_ROOT/tools/ml/finalize_nevergone_controls_aux_field_recovery_v16.py"; do [[ -f "$f" ]] || { echo "missing: $f" >&2; exit 2; }; done

echo "== regenerate v15 baseline =="
bash "$REPO_ROOT/tools/ml/run_nevergone_controlsbutton_slot80_field_recovery_v15.sh" "$APK" "$FIELD" "$OPERAND"
for f in "$IN15" "$Q15" "$GATED" "$FACTORY" "$CAT" "$RT"; do [[ -f "$f" ]] || { echo "missing generated: $f" >&2; exit 2; }; done

echo; echo "== prove Controls+0x124/+0x174 packaged stability =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_aux_field_stability.py" "$FIELD" "$GATED" "$FACTORY" --output "$ST"
before=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$Q15" || true)
python3 "$REPO_ROOT/tools/ml/finalize_nevergone_controls_aux_field_recovery_v16.py" "$IN15" "$Q15" "$ST" "$CAT" --output "$OUT" --queries-output "$OQ" --details-output "$DET"
after=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$OQ" || true)
[[ "$after" -eq $((before+3)) ]] || { echo "v16 query delta mismatch: $before -> $after" >&2; exit 2; }
python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" "$CAT" "$OQ" --output "$RES"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" "$RT" "$OUT" --details-output "$FAIL" --samples 60 | tee "$AUD"
python3 - "$IN15" "$OUT" "$DET" <<'PY'
import json,sys
want={('libcocos2dcpp.so:002a74c0',3):('CCJoyStick','0x124'),('libcocos2dcpp.so:002a7696',0):('CCJoyStick','0x124'),('libcocos2dcpp.so:002a7696',1):('cocos2d::CCSprite','0x174')}
def rd(p):return [json.loads(x) for x in open(p,encoding='utf-8') if x.strip()]
b={ (r.get('source_id'),r.get('call_index')):r for r in rd(sys.argv[1])};a={ (r.get('source_id'),r.get('call_index')):r for r in rd(sys.argv[2])}
if set(a)!=set(b):raise SystemExit('identity set changed')
for k in b:
 if k not in want and a[k]!=b[k]:raise SystemExit(f'non-target changed {k}')
for k,(cls,field) in want.items():
 if b[k].get('status')!='unresolved_receiver_class' or a[k].get('status')!='resolved_unique_vtable' or a[k].get('class_name')!=cls:raise SystemExit(f'bad transition {k}')
 rec=a[k].get('controls_aux_field_recovery_v16') or {}
 if rec.get('accepted') is not True or rec.get('receiver_field_offset')!=field:raise SystemExit(f'bad proof {k}')
d=rd(sys.argv[3]);
if len(d)!=3 or any(x.get('accepted') is not True for x in d):raise SystemExit('details mismatch')
print('v16 three-call recovery gate: pass')
PY

tar -C "$WORK_ROOT" -czf "$ARCH" controls-aux-field-stability-v16.jsonl controls-aux-field-recovery-v16.jsonl static-receiver-vtable-inference-v16.jsonl static-receiver-vtable-queries-v16.jsonl static-receiver-vtable-resolution-v16.jsonl receiver-type-failures-v16.jsonl receiver-type-failure-audit-v16.txt slot80-factory-field-candidates-v15.jsonl
printf '\narchive: %s\nquery delta: +3\nclasses: CCJoyStick + cocos2d::CCSprite\nwhole-program alias exclusion: no\n' "$ARCH"
