#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

if [[ $# -ne 3 ]]; then
  echo "Usage: bash $0 /path/to/com.hippiegame.nevergone.apk /path/to/nevergone-controls-field-alias-evidence.tar.gz /path/to/nevergone-controls-alias-operand-roots.tar.gz" >&2
  exit 2
fi

APK="$1"
FIELD_ALIAS_ARCHIVE="$2"
OPERAND_ARCHIVE="$3"
INFERENCE_V11="$WORK_ROOT/static-receiver-vtable-inference-v11.jsonl"
SLICES="$WORK_ROOT/evidence/mixed-callind-receiver-002a7594.tsv"
INFERENCE_V13="$WORK_ROOT/static-receiver-vtable-inference-v13.jsonl"
QUERIES_V13="$WORK_ROOT/static-receiver-vtable-queries-v13.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
STABILITY="$WORK_ROOT/controlsbutton-packaged-field-stability.jsonl"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v14.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v14.jsonl"
DETAILS="$WORK_ROOT/controlsbutton-field-recovery-v14.jsonl"
RESOLUTION="$WORK_ROOT/static-receiver-vtable-resolution-v14.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v14.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v14.txt"
ARCHIVE="$HOME/nevergone-controlsbutton-field-recovery-v14.tar.gz"

for required in "$APK" "$FIELD_ALIAS_ARCHIVE" "$OPERAND_ARCHIVE" \
  "$REPO_ROOT/tools/ml/run_nevergone_ccjoystick_field_recovery_v13.sh" \
  "$REPO_ROOT/tools/ml/run_nevergone_mixed_receiver_value_probe.sh" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controlsbutton_packaged_field_stability.py" \
  "$REPO_ROOT/tools/ml/finalize_nevergone_controlsbutton_field_recovery_v14.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

# Regenerate the current production baseline first. v13 itself rebuilds v12/v11
# and gates that calls 3/4/7 remain quarantined.
echo "== regenerate v13 baseline =="
bash "$REPO_ROOT/tools/ml/run_nevergone_ccjoystick_field_recovery_v13.sh"

# Regenerate actual nested CALLIND receiver slices. Never reuse the old
# GameSceneUI/Controls caller-argument convergence as receiver provenance.
echo
echo "== regenerate exact mixed receiver slices =="
bash "$REPO_ROOT/tools/ml/run_nevergone_mixed_receiver_value_probe.sh"

for required in "$INFERENCE_V11" "$SLICES" "$INFERENCE_V13" "$QUERIES_V13" \
  "$CATALOG" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing generated prerequisite: $required" >&2; exit 2; }
done

# One exact-APK packaged-program closure. It re-runs native local STORE,
# retained callback/delegate, scheduler/action, parent/touch, script-engine,
# dlsym, live-vptr and external-base-call checks internally.
echo
echo "== prove packaged ControlsButton field stability =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_controlsbutton_packaged_field_stability.py" \
  "$APK" "$FIELD_ALIAS_ARCHIVE" "$OPERAND_ARCHIVE" \
  "$INFERENCE_V11" "$SLICES" --details-output "$STABILITY"

before_queries=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$QUERIES_V13" || true)

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_controlsbutton_field_recovery_v14.py" \
  "$INFERENCE_V13" "$QUERIES_V13" "$STABILITY" "$CATALOG" \
  --output "$OUT" --queries-output "$OUTQ" --details-output "$DETAILS"

after_queries=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$OUTQ" || true)
[[ "$after_queries" -eq $((before_queries + 3)) ]] || {
  echo "v14 query count did not increase by exactly 3: $before_queries -> $after_queries" >&2
  exit 2
}

echo
echo "== resolve v14 queries =="
python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CATALOG" "$OUTQ" --output "$RESOLUTION"

echo
echo "== audit v14 receiver failures =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RECEIVER_TYPES" "$OUT" --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

python3 - "$OUT" "$OUTQ" "$STABILITY" <<'PY'
import json,sys
source="libcocos2dcpp.so:002a7594"
want={3:("002a7612","0x134"),4:("002a7634","0x130"),7:("002a7686","0x12c")}
rows=[json.loads(x) for x in open(sys.argv[1],encoding="utf-8") if x.strip()]
by={(r.get("source_id"),r.get("call_index")):r for r in rows}
for call,(address,field) in want.items():
    r=by.get((source,call))
    if not r or r.get("status")!="resolved_unique_vtable" or r.get("call_address")!=address \
       or r.get("class_name")!="ControlsButton" \
       or r.get("receiver_vtable_id")!="libcocos2dcpp.so:008f46f8":
        raise SystemExit(f"v14 recovery mismatch call {call}: {r}")
    recovery=r.get("controlsbutton_field_recovery_v14") or {}
    if recovery.get("receiver_field_offset")!=field or recovery.get("accepted") is not True \
       or recovery.get("caller_argument_provenance_used") is not False:
        raise SystemExit(f"v14 recovery provenance mismatch call {call}: {recovery}")
queries=[json.loads(x) for x in open(sys.argv[2],encoding="utf-8") if x.strip()]
q={(r.get("source_id"),r.get("call_index")):r for r in queries}
for call in want:
    r=q.get((source,call))
    if not r or r.get("receiver_vtable_id")!="libcocos2dcpp.so:008f46f8" \
       or r.get("dispatch_slot_offset")!="0x80":
        raise SystemExit(f"v14 query missing/mismatched call {call}: {r}")
st=[json.loads(x) for x in open(sys.argv[3],encoding="utf-8") if x.strip()]
s=st[0]
if s.get("packaged_controlsbutton_field_stability_proven") is not True \
   or s.get("whole_program_alias_writes_excluded") is not False \
   or s.get("external_injected_code_excluded") is not False:
    raise SystemExit(f"v14 packaged-scope contract mismatch: {s}")
print("v14 three-call recovery gate: pass")
PY

tar -C "$WORK_ROOT" -czf "$ARCHIVE" \
  controlsbutton-packaged-field-stability.jsonl \
  controlsbutton-field-recovery-v14.jsonl \
  static-receiver-vtable-inference-v14.jsonl \
  static-receiver-vtable-queries-v14.jsonl \
  static-receiver-vtable-resolution-v14.jsonl \
  receiver-type-failures-v14.jsonl \
  receiver-type-failure-audit-v14.txt

printf '\nv14 inference: %s\n' "$OUT"
printf 'v14 queries: %s\n' "$OUTQ"
printf 'v14 packaged field stability: %s\n' "$STABILITY"
printf 'v14 recovery details: %s\n' "$DETAILS"
printf 'v14 resolution: %s\n' "$RESOLUTION"
printf 'v14 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'archive: %s\n' "$ARCHIVE"
printf 'production query delta: +3\n'
printf 'caller-argument quarantine reused: no\n'
