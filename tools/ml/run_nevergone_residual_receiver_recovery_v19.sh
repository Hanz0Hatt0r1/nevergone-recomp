#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

IN17="$WORK_ROOT/static-receiver-vtable-inference-v17.jsonl"
Q17="$WORK_ROOT/static-receiver-vtable-queries-v17.jsonl"
CAT="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RT="$WORK_ROOT/indirect-receiver-types.tsv"
SLICE_DIR="$WORK_ROOT/evidence/non-slot80-receiver-frontier-v18"
STORES="$WORK_ROOT/ccjoystick-init-field-stores-v19.tsv"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v19.jsonl"
OQ="$WORK_ROOT/static-receiver-vtable-queries-v19.jsonl"
DET="$WORK_ROOT/residual-receiver-recovery-v19.jsonl"
RES="$WORK_ROOT/static-receiver-vtable-resolution-v19.jsonl"
FAIL="$WORK_ROOT/receiver-type-failures-v19.jsonl"
AUD="$WORK_ROOT/receiver-type-failure-audit-v19.txt"
ARCH="$HOME/nevergone-residual-receiver-recovery-v19.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in \
  "$IN17" "$Q17" "$CAT" "$RT" \
  "$SLICE_DIR/libcocos2dcpp.so_002a08fc.tsv" \
  "$SLICE_DIR/libcocos2dcpp.so_002a647e.tsv" \
  "$REPO_ROOT/tools/ghidra/ExportExactParameterFieldFactoryStore.java" \
  "$REPO_ROOT/tools/ml/finalize_nevergone_residual_receiver_recovery_v19.py" \
  "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py"; do
  [[ -e "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }

mkdir -p "$WORK_ROOT"

echo "== export CCJoyStick::init exact field stores =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportExactParameterFieldFactoryStore.java 002a6584 "$STORES"

before=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$Q17" || true)
python3 "$REPO_ROOT/tools/ml/finalize_nevergone_residual_receiver_recovery_v19.py" \
  "$IN17" "$Q17" "$SLICE_DIR" "$STORES" "$CAT" \
  --output "$OUT" --queries-output "$OQ" --details-output "$DET"
after=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$OQ" || true)
[[ "$after" -eq $((before + 5)) ]] || {
  echo "v19 query delta mismatch: $before -> $after" >&2
  exit 2
}

python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CAT" "$OQ" --output "$RES"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RT" "$OUT" --details-output "$FAIL" --samples 60 | tee "$AUD"

python3 - "$IN17" "$OUT" "$DET" "$Q17" "$OQ" <<'PY'
import json, sys

targets = {
    ("libcocos2dcpp.so:002a08fc", 0): ("AndroidSdkMgr", "libcocos2dcpp.so:008f3ce8"),
    ("libcocos2dcpp.so:002a647e", 0): ("cocos2d::CCSprite", "libcocos2dcpp.so:009363d8"),
    ("libcocos2dcpp.so:002a647e", 2): ("cocos2d::CCSprite", "libcocos2dcpp.so:009363d8"),
    ("libcocos2dcpp.so:002a647e", 3): ("cocos2d::CCSprite", "libcocos2dcpp.so:009363d8"),
    ("libcocos2dcpp.so:002a647e", 5): ("cocos2d::CCSprite", "libcocos2dcpp.so:009363d8"),
}
residual = {
    ("libcocos2dcpp.so:002a47c6", 6),
    ("libcocos2dcpp.so:002a5dd0", 0),
}

def rd(path):
    with open(path, encoding="utf-8") as f:
        return [json.loads(line) for line in f if line.strip()]

def by(rows):
    return {(r.get("source_id"), r.get("call_index")): r for r in rows}

old=by(rd(sys.argv[1])); new=by(rd(sys.argv[2]))
if set(old)!=set(new): raise SystemExit("v19 inference identity set changed")
for key in old:
    if key not in targets and new[key] != old[key]:
        raise SystemExit(f"v19 non-target changed: {key}")
for key,(cls,vt) in targets.items():
    if old[key].get("status") != "unresolved_receiver_class":
        raise SystemExit(f"v19 target baseline changed: {key}")
    row=new[key]
    if row.get("status") != "resolved_unique_vtable" or row.get("class_name") != cls or row.get("receiver_vtable_id") != vt:
        raise SystemExit(f"v19 target not exactly resolved: {key}: {row}")
    rec=row.get("residual_receiver_recovery_v19") or {}
    if rec.get("accepted") is not True or rec.get("reasons") != []:
        raise SystemExit(f"v19 proof not accepted: {key}: {rec}")

details=rd(sys.argv[3])
if len(details)!=5 or any(r.get("accepted") is not True or r.get("reasons") != [] for r in details):
    raise SystemExit("v19 detail acceptance mismatch")

q0={(r.get("source_id"),r.get("call_index")) for r in rd(sys.argv[4])}
q1={(r.get("source_id"),r.get("call_index")) for r in rd(sys.argv[5])}
if q1-q0 != set(targets):
    raise SystemExit(f"v19 exact query delta mismatch: {sorted(q1-q0)}")

remaining={(r.get("source_id"),r.get("call_index")) for r in new.values() if r.get("status")=="unresolved_receiver_class"}
if remaining != residual:
    raise SystemExit(f"v19 residual frontier mismatch: {sorted(remaining)}")

print("v19 five-call residual receiver recovery gate: pass")
print("remaining unresolved receiver classes:", sorted(residual))
PY

items=(
  static-receiver-vtable-inference-v19.jsonl
  static-receiver-vtable-queries-v19.jsonl
  static-receiver-vtable-resolution-v19.jsonl
  residual-receiver-recovery-v19.jsonl
  receiver-type-failures-v19.jsonl
  receiver-type-failure-audit-v19.txt
  ccjoystick-init-field-stores-v19.tsv
  static-vtable-slot-catalog.jsonl
)
for item in "${items[@]}"; do
  [[ -f "$WORK_ROOT/$item" ]] || { echo "missing archive member: $WORK_ROOT/$item" >&2; exit 2; }
done

tar -C "$WORK_ROOT" -czf "$ARCH" \
  "${items[@]}" \
  evidence/non-slot80-receiver-frontier-v18

printf '\narchive: %s\nquery delta: +5\nremaining unresolved receiver classes: 2\naffects routing: no\n' "$ARCH"
