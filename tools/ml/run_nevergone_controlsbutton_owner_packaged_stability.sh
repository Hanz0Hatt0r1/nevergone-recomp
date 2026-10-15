#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <original-apk> <controlsbutton-owner-field-evidence.tar.gz>" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APK="$1"
OWNER_ARCHIVE="$2"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${OUT_ARCHIVE:=$HOME/nevergone-controlsbutton-owner-packaged-stability.tar.gz}"
mkdir -p "$WORK_ROOT"

FIRST="$WORK_ROOT/controlsbutton-owner-first-hop-audit.jsonl"
FIRST_TEXT="$WORK_ROOT/controlsbutton-owner-first-hop-audit.txt"
PARENT="$WORK_ROOT/controlsbutton-parent-script-static-closure.jsonl"
PARENT_TEXT="$WORK_ROOT/controlsbutton-parent-script-static-closure.txt"
DLSYM="$WORK_ROOT/controlsbutton-dlsym-closure.jsonl"
DLSYM_TEXT="$WORK_ROOT/controlsbutton-dlsym-closure.txt"
DETAILS="$WORK_ROOT/controlsbutton-owner-packaged-stability.jsonl"
TEXT="$WORK_ROOT/controlsbutton-owner-packaged-stability.txt"

printf '%s\n' '== classify ControlsButton owner first hop =='
WORK_ROOT="$WORK_ROOT" OUT_ARCHIVE="$WORK_ROOT/controlsbutton-owner-first-hop.tar.gz" \
  bash "$ROOT/tools/ml/run_nevergone_controlsbutton_owner_first_hop.sh" \
  "$APK" "$OWNER_ARCHIVE"

printf '\n%s\n' '== close inherited parent/script packaged channel =='
python3 "$ROOT/tools/ml/audit_nevergone_controls_parent_script_static_closure.py" \
  "$APK" --details-output "$PARENT" | tee "$PARENT_TEXT"

printf '\n%s\n' '== close packaged dlsym setter surface =='
python3 "$ROOT/tools/ml/audit_nevergone_controls_dlsym_closure.py" \
  "$APK" --details-output "$DLSYM" | tee "$DLSYM_TEXT"

printf '\n%s\n' '== prove ControlsButton packaged owner-field stability =='
python3 "$ROOT/tools/ml/audit_nevergone_controlsbutton_owner_packaged_stability.py" \
  "$APK" "$OWNER_ARCHIVE" "$FIRST" "$PARENT" "$DLSYM" \
  --details-output "$DETAILS" | tee "$TEXT"

python3 - "$DETAILS" <<'PY'
import json, sys
from pathlib import Path
r = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
assert r["format"] == "NEVERGONE.ControlsButtonOwnerFieldPackagedStability/1"
assert r["scope"] == "exact_packaged_apk_normal_code_execution"
assert r["target_fields"] == ["0x104", "0x108", "0x10c"]
assert r["first_hop_closed"] is True
assert r["owner_fields_stable"] is True
assert r["receiver_class_proven"] is False
assert r["whole_program_alias_writes_excluded"] is False
assert r["external_injected_code_excluded"] is False
assert r["affects_routing"] is False
PY

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
for f in "$FIRST" "$FIRST_TEXT" "$PARENT" "$PARENT_TEXT" "$DLSYM" "$DLSYM_TEXT" "$DETAILS" "$TEXT"; do
  cp "$f" "$tmp/"
done
tar -czf "$OUT_ARCHIVE" -C "$tmp" .

echo
echo "Archive: $OUT_ARCHIVE"
echo "Original APK included: no"
echo "Owner evidence archive included: no"
echo "Receiver class proven: no"
echo "Affects routing: no"
