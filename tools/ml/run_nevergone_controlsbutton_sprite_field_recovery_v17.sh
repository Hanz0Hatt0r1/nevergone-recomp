#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

if [[ $# -ne 4 ]]; then
  echo "Usage: bash $0 APK field-alias-evidence.tar.gz operand-roots.tar.gz controlsbutton-owner-field-evidence.tar.gz" >&2
  exit 2
fi

APK="$1"
FIELD="$2"
OPERAND="$3"
OWNER="$4"

IN16="$WORK_ROOT/static-receiver-vtable-inference-v16.jsonl"
Q16="$WORK_ROOT/static-receiver-vtable-queries-v16.jsonl"
FACTORY="$WORK_ROOT/slot80-factory-field-candidates-v15.jsonl"
CAT="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RT="$WORK_ROOT/indirect-receiver-types.tsv"
ST="$WORK_ROOT/controlsbutton-owner-packaged-stability.jsonl"
STTXT="$WORK_ROOT/controlsbutton-owner-packaged-stability.txt"
OUT="$WORK_ROOT/static-receiver-vtable-inference-v17.jsonl"
OQ="$WORK_ROOT/static-receiver-vtable-queries-v17.jsonl"
DET="$WORK_ROOT/controlsbutton-sprite-field-recovery-v17.jsonl"
RES="$WORK_ROOT/static-receiver-vtable-resolution-v17.jsonl"
FAIL="$WORK_ROOT/receiver-type-failures-v17.jsonl"
AUD="$WORK_ROOT/receiver-type-failure-audit-v17.txt"
ARCH="$HOME/nevergone-controlsbutton-sprite-field-recovery-v17.tar.gz"
OWNER_PROOF_ARCH="$WORK_ROOT/controlsbutton-owner-packaged-stability-proof.tar.gz"

for f in \
  "$APK" "$FIELD" "$OPERAND" "$OWNER" \
  "$REPO_ROOT/tools/ml/run_nevergone_controls_aux_field_recovery_v16.sh" \
  "$REPO_ROOT/tools/ml/run_nevergone_controlsbutton_owner_packaged_stability.sh" \
  "$REPO_ROOT/tools/ml/finalize_nevergone_controlsbutton_sprite_field_recovery_v17.py"; do
  [[ -f "$f" ]] || { echo "missing: $f" >&2; exit 2; }
done

echo "== regenerate v16 baseline =="
bash "$REPO_ROOT/tools/ml/run_nevergone_controls_aux_field_recovery_v16.sh" \
  "$APK" "$FIELD" "$OPERAND"
for f in "$IN16" "$Q16" "$FACTORY" "$CAT" "$RT"; do
  [[ -f "$f" ]] || { echo "missing generated: $f" >&2; exit 2; }
done

echo
echo "== prove ControlsButton+0x104/+0x108/+0x10c packaged owner stability =="
WORK_ROOT="$WORK_ROOT" OUT_ARCHIVE="$OWNER_PROOF_ARCH" \
  bash "$REPO_ROOT/tools/ml/run_nevergone_controlsbutton_owner_packaged_stability.sh" \
  "$APK" "$OWNER"
[[ -f "$ST" ]] || { echo "missing generated owner stability: $ST" >&2; exit 2; }

before=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$Q16" || true)
python3 "$REPO_ROOT/tools/ml/finalize_nevergone_controlsbutton_sprite_field_recovery_v17.py" \
  "$IN16" "$Q16" "$ST" "$FACTORY" "$CAT" \
  --output "$OUT" --queries-output "$OQ" --details-output "$DET"
after=$(grep -c '"format": "NEVERGONE.StaticReceiverVtableQuery/1"' "$OQ" || true)
[[ "$after" -eq $((before + 3)) ]] || {
  echo "v17 query delta mismatch: $before -> $after" >&2
  exit 2
}

python3 "$REPO_ROOT/tools/ml/resolve_nevergone_receiver_queries_v11.py" \
  "$CAT" "$OQ" --output "$RES"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures_v11.py" \
  "$RT" "$OUT" --details-output "$FAIL" --samples 60 | tee "$AUD"

python3 - "$IN16" "$OUT" "$DET" "$Q16" "$OQ" <<'PY'
import json
import sys

want = {
    ("libcocos2dcpp.so:002a87d2", 3): "0x104",
    ("libcocos2dcpp.so:002a87d2", 4): "0x10c",
    ("libcocos2dcpp.so:002a87d2", 5): "0x108",
}

def rd(path):
    with open(path, encoding="utf-8") as handle:
        return [json.loads(line) for line in handle if line.strip()]

before = {(r.get("source_id"), r.get("call_index")): r for r in rd(sys.argv[1])}
after = {(r.get("source_id"), r.get("call_index")): r for r in rd(sys.argv[2])}
if set(before) != set(after):
    raise SystemExit("v17 inference identity set changed")

for key in before:
    if key not in want and after[key] != before[key]:
        raise SystemExit(f"v17 non-target changed: {key}")

for key, field in want.items():
    old = before[key]
    new = after[key]
    if old.get("status") != "ambiguous_receiver_class":
        raise SystemExit(f"v17 target baseline status changed: {key} -> {old.get('status')}")
    if new.get("status") != "resolved_unique_vtable":
        raise SystemExit(f"v17 target not resolved: {key}")
    if new.get("class_name") != "cocos2d::CCSprite":
        raise SystemExit(f"v17 target class mismatch: {key}")
    rec = new.get("controlsbutton_sprite_field_recovery_v17") or {}
    if rec.get("accepted") is not True or rec.get("receiver_field_offset") != field:
        raise SystemExit(f"v17 proof mismatch: {key}")

rows = rd(sys.argv[3])
if len(rows) != 3 or any(row.get("accepted") is not True for row in rows):
    raise SystemExit("v17 details mismatch")

q_before = {(r.get("source_id"), r.get("call_index")) for r in rd(sys.argv[4])}
q_after = {(r.get("source_id"), r.get("call_index")) for r in rd(sys.argv[5])}
if q_after - q_before != set(want):
    raise SystemExit(f"v17 exact query delta mismatch: {sorted(q_after - q_before)}")

print("v17 three-call ControlsButton sprite recovery gate: pass")
PY

items=(
  controlsbutton-owner-packaged-stability.jsonl
  controlsbutton-owner-packaged-stability.txt
  controlsbutton-sprite-field-recovery-v17.jsonl
  static-receiver-vtable-inference-v17.jsonl
  static-receiver-vtable-queries-v17.jsonl
  static-receiver-vtable-resolution-v17.jsonl
  receiver-type-failures-v17.jsonl
  receiver-type-failure-audit-v17.txt
  slot80-factory-field-candidates-v15.jsonl
)
for item in "${items[@]}"; do
  [[ -f "$WORK_ROOT/$item" ]] || { echo "missing archive member: $WORK_ROOT/$item" >&2; exit 2; }
done

tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}"
printf '\narchive: %s\nquery delta: +3\nclass: cocos2d::CCSprite\nsource: 002a87d2 calls 3/4/5\nwhole-program alias exclusion: no\n' "$ARCH"
