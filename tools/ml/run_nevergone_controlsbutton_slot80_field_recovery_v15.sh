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
IN14="$WORK_ROOT/static-receiver-vtable-inference-v14.jsonl"
Q14="$WORK_ROOT/static-receiver-vtable-queries-v14.jsonl"
STABILITY="$WORK_ROOT/controlsbutton-packaged-field-stability.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
PLAN="$WORK_ROOT/slot80-receiver-slice-plan.tsv"
SLICE_DIR="$WORK_ROOT/evidence/slot80-receiver-slices"
FACTORY="$WORK_ROOT/slot80-factory-field-candidates-v15.jsonl"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v15.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v15.jsonl"
DETAILS="$WORK_ROOT/controlsbutton-slot80-field-recovery-v15.jsonl"
RESOLUTION="$WORK_ROOT/static-receiver-vtable-resolution-v15.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v15.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v15.txt"
ARCHIVE="$HOME/nevergone-controlsbutton-slot80-field-recovery-v15.tar.gz"

for required in "$APK" "$FIELD_ALIAS_ARCHIVE" "$OPERAND_ARCHIVE" \
  "$REPO_ROOT/tools/ml/run_nevergone_controlsbutton_field_recovery_v14.sh" \
  "$REPO_ROOT/tools/ml/run_nevergone_slot80_receiver_slice_probe.sh" \
  "$REPO_ROOT/tools/ml/audit_nevergone_slot80_factory_fields.py" \
  "$REPO_ROOT/tools/ml/finalize_nevergone_controlsbutton_slot80_field_recovery_v15.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

# v14 regenerates v13/v11, the mixed actual-receiver slice, the exact packaged
# field stability proof, the independent 14-site full-this closure and the
# three quarantined ControlsButton recoveries.
echo "== regenerate gated v14 baseline =="
bash "$REPO_ROOT/tools/ml/run_nevergone_controlsbutton_field_recovery_v14.sh" \
  "$APK" "$FIELD_ALIAS_ARCHIVE" "$OPERAND_ARCHIVE"

for required in "$IN14" "$Q14" "$STABILITY" "$CATALOG" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing v14 prerequisite: $required" >&2; exit 2; }
done

# Re-export the older 17 complete slot-0x80 actual receiver slices. This is
# separate from the three mixed calls promoted in v14.
echo
echo "== regenerate 17 slot-0x80 structural receiver slices =="
bash "$REPO_ROOT/tools/ml/run_nevergone_slot80_receiver_slice_probe.sh"

for required in "$PLAN" "$SLICE_DIR"; do
  [[ -e "$required" ]] || { echo "missing slot80 prerequisite: $required" >&2; exit 2; }
done

# Rejoin the exact receiver field offsets with the pinned factory-return STOREs
# and current catalog. This remains candidate evidence until v15 combines it
# with v14's packaged stability proof for the three ControlsButton fields.
echo
echo "== regenerate 17 construction-linked factory-field candidates =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_slot80_factory_fields.py" \
  "$APK" "$PLAN" "$SLICE_DIR" "$CATALOG" --details-output "$FACTORY"

before_queries=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$Q14" || true)

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_controlsbutton_slot80_field_recovery_v15.py" \
  "$IN14" "$Q14" "$STABILITY" "$FACTORY" "$CATALOG" \
  --output "$OUT" --queries-output "$OUTQ" --details-output "$DETAILS"

after_queries=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$OUTQ" || true)
[[ "$after_queries" -eq $((before_queries + 11)) ]] || {
  echo "v15 query count did not increase by exactly 11: $before_queries -> $after_queries" >&2
  exit 2
}

echo
echo "== resolve v15 queries =="
python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CATALOG" "$OUTQ" --output "$RESOLUTION"

echo
echo "== audit v15 receiver failures =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RECEIVER_TYPES" "$OUT" --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

python3 - "$IN14" "$OUT" "$OUTQ" "$DETAILS" <<'PY'
import json,sys
want={
 ("libcocos2dcpp.so:002a74c0",0):("002a74fc","0x12c"),
 ("libcocos2dcpp.so:002a74c0",1):("002a750a","0x130"),
 ("libcocos2dcpp.so:002a74c0",2):("002a7518","0x134"),
 ("libcocos2dcpp.so:002a7594",0):("002a75b4","0x12c"),
 ("libcocos2dcpp.so:002a7594",1):("002a75c2","0x130"),
 ("libcocos2dcpp.so:002a7594",2):("002a75d0","0x134"),
 ("libcocos2dcpp.so:002a7594",5):("002a764a","0x12c"),
 ("libcocos2dcpp.so:002a7594",6):("002a7658","0x134"),
 ("libcocos2dcpp.so:002a78cc",0):("002a78e6","0x12c"),
 ("libcocos2dcpp.so:002a78cc",1):("002a78f4","0x130"),
 ("libcocos2dcpp.so:002a78cc",2):("002a7902","0x134"),
}
def read(path): return [json.loads(x) for x in open(path,encoding="utf-8") if x.strip()]
before=read(sys.argv[1]); after=read(sys.argv[2]); queries=read(sys.argv[3]); details=read(sys.argv[4])
b={(r.get("source_id"),r.get("call_index")):r for r in before}
a={(r.get("source_id"),r.get("call_index")):r for r in after}
if set(b)!=set(a): raise SystemExit("v15 changed inference identity set")
for k in b:
    if k not in want and b[k]!=a[k]: raise SystemExit(f"v15 changed non-target row: {k}")
for k,(address,field) in want.items():
    old,new=b[k],a[k]
    if old.get("status")!="unresolved_receiver_class": raise SystemExit(f"v14 target not unresolved: {k}")
    if new.get("status")!="resolved_unique_vtable" or new.get("class_name")!="ControlsButton" \
       or new.get("receiver_vtable_id")!="libcocos2dcpp.so:008f46f8" or new.get("call_address")!=address:
        raise SystemExit(f"v15 target mismatch: {k}: {new}")
    rec=new.get("controlsbutton_slot80_field_recovery_v15") or {}
    if rec.get("accepted") is not True or rec.get("receiver_field_offset")!=field \
       or rec.get("packaged_field_stability_proven") is not True:
        raise SystemExit(f"v15 provenance mismatch: {k}: {rec}")
q={(r.get("source_id"),r.get("call_index")):r for r in queries}
for k in want:
    if k not in q or q[k].get("receiver_vtable_id")!="libcocos2dcpp.so:008f46f8" \
       or q[k].get("dispatch_slot_offset")!="0x80":
        raise SystemExit(f"v15 query missing/mismatched: {k}")
if len(details)!=11 or any(r.get("accepted") is not True for r in details):
    raise SystemExit("v15 details do not contain exactly 11 accepted recoveries")
print("v15 eleven-call recovery gate: pass")
PY

tar -C "$WORK_ROOT" -czf "$ARCHIVE" \
  controlsbutton-packaged-field-stability.jsonl \
  slot80-factory-field-candidates-v15.jsonl \
  controlsbutton-slot80-field-recovery-v15.jsonl \
  static-receiver-vtable-inference-v15.jsonl \
  static-receiver-vtable-queries-v15.jsonl \
  static-receiver-vtable-resolution-v15.jsonl \
  receiver-type-failures-v15.jsonl \
  receiver-type-failure-audit-v15.txt \
  slot80-receiver-slice-plan.tsv \
  slot80-receiver-slice-audit.jsonl \
  slot80-receiver-slice-audit.txt \
  evidence/slot80-receiver-slices

printf '\nv15 inference: %s\n' "$OUT"
printf 'v15 queries: %s\n' "$OUTQ"
printf 'v15 details: %s\n' "$DETAILS"
printf 'v15 resolution: %s\n' "$RESOLUTION"
printf 'v15 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'archive: %s\n' "$ARCHIVE"
printf 'production query delta: +11\n'
printf 'promoted class: ControlsButton only\n'
printf 'CCJoyStick/CCSprite promoted: no\n'
