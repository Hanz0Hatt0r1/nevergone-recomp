#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

# Always regenerate v12 from the quarantined v11 baseline first.
echo "== regenerate current v12 baseline =="
bash "$REPO_ROOT/tools/ml/run_nevergone_exact_singleton_cell_recovery_v12.sh"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v12.jsonl"
QUERIES="$WORK_ROOT/static-receiver-vtable-queries-v12.jsonl"
FIELD_AUDIT="$WORK_ROOT/ccjoystick-field-factory-provenance-audit.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v13.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v13.jsonl"
DETAILS="$WORK_ROOT/ccjoystick-field-recovery-v13.jsonl"
RESOLUTION="$WORK_ROOT/static-receiver-vtable-resolution-v13.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v13.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v13.txt"

for required in "$INFERENCE" "$QUERIES" "$FIELD_AUDIT" "$CATALOG" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

python3 - "$INFERENCE" "$QUERIES" <<'PY'
import json,sys
with open(sys.argv[1],encoding="utf-8") as f:
    rows=[json.loads(x) for x in f if x.strip()]
by={(r.get("source_id"),r.get("call_index")):r for r in rows}
target=("libcocos2dcpp.so:002a647e",4)
r=by.get(target)
if r is None or r.get("status")!="receiver_not_single_pointer" or r.get("dispatch_slot_offset")!="0x58":
    raise SystemExit(f"stale v12 CCJoyStick baseline row: {r}")
for call in (3,4,7):
    k=("libcocos2dcpp.so:002a7594",call)
    q=by.get(k)
    if q is None or q.get("status")!="receiver_not_single_pointer":
        raise SystemExit(f"v12 quarantine changed for {k}: {q}")
with open(sys.argv[2],encoding="utf-8") as f:
    qkeys={(q.get("source_id"),q.get("call_index")) for q in (json.loads(x) for x in f if x.strip())}
if target in qkeys:
    raise SystemExit("unexpected v12 query for CCJoyStick field row")
if any(("libcocos2dcpp.so:002a7594",c) in qkeys for c in (3,4,7)):
    raise SystemExit("v12 quarantine query unexpectedly present")
print("v12 CCJoyStick/quarantine baseline gate: pass")
PY

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_ccjoystick_field_recovery_v13.py" \
  "$INFERENCE" "$QUERIES" "$FIELD_AUDIT" "$CATALOG" \
  --output "$OUT" --queries-output "$OUTQ" --details-output "$DETAILS"

echo
echo "receiver query resolution after CCJoyStick field recovery:"
python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CATALOG" "$OUTQ" --output "$RESOLUTION"

echo
echo "virtual receiver failure audit after CCJoyStick field recovery:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RECEIVER_TYPES" "$OUT" --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

python3 - "$OUT" "$OUTQ" <<'PY'
import json,sys
with open(sys.argv[1],encoding="utf-8") as f:
    rows=[json.loads(x) for x in f if x.strip()]
by={(r.get("source_id"),r.get("call_index")):r for r in rows}
target=("libcocos2dcpp.so:002a647e",4)
r=by.get(target)
if r is None or r.get("status")!="resolved_unique_vtable" or r.get("class_name")!="cocos2d::CCSprite" or r.get("receiver_vtable_id")!="libcocos2dcpp.so:009363d8":
    raise SystemExit(f"v13 CCJoyStick field recovery identity mismatch: {r}")
for call in (3,4,7):
    k=("libcocos2dcpp.so:002a7594",call)
    q=by.get(k)
    if q is None or q.get("status")!="receiver_not_single_pointer":
        raise SystemExit(f"v13 revived quarantined receiver {k}: {q}")
with open(sys.argv[2],encoding="utf-8") as f:
    queries=[json.loads(x) for x in f if x.strip()]
qkeys={(q.get("source_id"),q.get("call_index")) for q in queries}
if target not in qkeys:
    raise SystemExit("v13 CCSprite query missing")
if any(("libcocos2dcpp.so:002a7594",c) in qkeys for c in (3,4,7)):
    raise SystemExit("v13 emitted query for quarantined 002a7594 call")
print("v13 CCJoyStick recovery/quarantine gate: pass")
PY

printf '\nv13 inference: %s\n' "$OUT"
printf 'v13 queries: %s\n' "$OUTQ"
printf 'v13 CCJoyStick recovery details: %s\n' "$DETAILS"
printf 'v13 query resolution: %s\n' "$RESOLUTION"
printf 'v13 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
