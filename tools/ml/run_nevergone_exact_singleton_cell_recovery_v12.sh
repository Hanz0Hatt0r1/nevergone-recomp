#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

# Always regenerate v11 with the current quarantine logic before applying v12.
# This prevents a stale pre-#568 v11 file from reviving the three invalid
# caller-argument mixed-receiver recoveries.
echo "== regenerate quarantined v11 baseline =="
bash "$REPO_ROOT/tools/ml/run_nevergone_mixed_slot_target_recovery_v11.sh"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v11.jsonl"
QUERIES="$WORK_ROOT/static-receiver-vtable-queries-v11.jsonl"
SINGLETON_AUDIT="$WORK_ROOT/exact-singleton-cell-provenance-audit.jsonl"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v12.jsonl"
OUTQ="$WORK_ROOT/static-receiver-vtable-queries-v12.jsonl"
DETAILS="$WORK_ROOT/exact-singleton-cell-recovery-v12.jsonl"
RESOLUTION="$WORK_ROOT/static-receiver-vtable-resolution-v12.jsonl"
FAILURES="$WORK_ROOT/receiver-type-failures-v12.jsonl"
FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v12.txt"

for required in "$INFERENCE" "$QUERIES" "$SINGLETON_AUDIT" "$CATALOG" "$RECEIVER_TYPES"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

# Gate the expected post-quarantine baseline before v12. We intentionally do
# not assert total unresolved counts here because independent non-singleton
# evidence work may evolve, but these five identities must remain unresolved:
# the two exact singleton rows plus the three quarantined 002a7594 calls.
python3 - "$INFERENCE" "$QUERIES" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    rows=[json.loads(x) for x in f if x.strip()]
by={(r.get("source_id"), r.get("call_index")): r for r in rows}
expected={
    ("libcocos2dcpp.so:0029edb4",0): "0x8",
    ("libcocos2dcpp.so:002a08fc",1): "0x8",
    ("libcocos2dcpp.so:002a7594",3): "0x80",
    ("libcocos2dcpp.so:002a7594",4): "0x80",
    ("libcocos2dcpp.so:002a7594",7): "0x80",
}
for key, slot in expected.items():
    row=by.get(key)
    if row is None:
        raise SystemExit(f"missing v11 baseline row: {key}")
    if row.get("status") != "receiver_not_single_pointer" or row.get("dispatch_slot_offset") != slot:
        raise SystemExit(f"stale/non-quarantined v11 row {key}: status={row.get('status')} slot={row.get('dispatch_slot_offset')}")
with open(sys.argv[2], encoding="utf-8") as f:
    queries=[json.loads(x) for x in f if x.strip()]
qkeys={(q.get("source_id"), q.get("call_index")) for q in queries}
for key in expected:
    if key in qkeys:
        raise SystemExit(f"unexpected v11 query for unresolved baseline row: {key}")
print("v11 exact-singleton/quarantine baseline gate: pass")
PY

python3 "$REPO_ROOT/tools/ml/finalize_nevergone_exact_singleton_cell_recovery.py" \
  "$INFERENCE" "$QUERIES" "$SINGLETON_AUDIT" "$CATALOG" \
  --output "$OUT" \
  --queries-output "$OUTQ" \
  --details-output "$DETAILS"

echo
echo "receiver query resolution after exact-singleton recovery:"
python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CATALOG" "$OUTQ" --output "$RESOLUTION"

echo
echo "virtual receiver failure audit after exact-singleton recovery:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RECEIVER_TYPES" "$OUT" \
  --details-output "$FAILURES" --samples 60 \
  | tee "$FAILURE_AUDIT"

# Regression gate: v12 may resolve exactly the two singleton cells, but the
# three caller-argument-confused 002a7594 calls must remain quarantined.
python3 - "$OUT" "$OUTQ" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    rows=[json.loads(x) for x in f if x.strip()]
by={(r.get("source_id"), r.get("call_index")): r for r in rows}
resolved={
    ("libcocos2dcpp.so:0029edb4",0): ("ActionDataManager", "libcocos2dcpp.so:008f3988"),
    ("libcocos2dcpp.so:002a08fc",1): ("AndroidSdkMgr", "libcocos2dcpp.so:008f3ce8"),
}
for key, (cls, vt) in resolved.items():
    row=by.get(key)
    if row is None or row.get("status") != "resolved_unique_vtable" or row.get("class_name") != cls or row.get("receiver_vtable_id") != vt:
        raise SystemExit(f"v12 exact-singleton recovery identity mismatch for {key}: {row}")
for call in (3,4,7):
    key=("libcocos2dcpp.so:002a7594",call)
    row=by.get(key)
    if row is None or row.get("status") != "receiver_not_single_pointer":
        raise SystemExit(f"v12 revived quarantined receiver {key}: {row}")
with open(sys.argv[2], encoding="utf-8") as f:
    queries=[json.loads(x) for x in f if x.strip()]
qkeys={(q.get("source_id"), q.get("call_index")) for q in queries}
if not set(resolved).issubset(qkeys):
    raise SystemExit("v12 singleton queries missing")
if any(("libcocos2dcpp.so:002a7594",call) in qkeys for call in (3,4,7)):
    raise SystemExit("v12 emitted query for quarantined 002a7594 call")
print("v12 singleton recovery/quarantine gate: pass")
PY

printf '\nv12 inference: %s\n' "$OUT"
printf 'v12 queries: %s\n' "$OUTQ"
printf 'v12 exact-singleton recovery details: %s\n' "$DETAILS"
printf 'v12 query resolution: %s\n' "$RESOLUTION"
printf 'v12 failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
