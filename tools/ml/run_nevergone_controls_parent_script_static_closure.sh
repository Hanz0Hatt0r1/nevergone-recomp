#!/usr/bin/env bash
set -euo pipefail
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

if [[ $# -ne 1 ]]; then
  echo "Usage: bash $0 /path/to/com.hippiegame.nevergone.apk" >&2
  exit 2
fi
APK="$1"
DETAILS="$WORK_ROOT/controls-parent-script-static-closure.jsonl"
AUDIT="$WORK_ROOT/controls-parent-script-static-closure.txt"

for required in "$APK" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_cocos_backrefs.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_parent_script_static_closure.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
mkdir -p "$WORK_ROOT"
PYTHONPATH="$REPO_ROOT/tools/ml${PYTHONPATH:+:$PYTHONPATH}" \
python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_parent_script_static_closure.py" \
  "$APK" --details-output "$DETAILS" | tee "$AUDIT"
printf '\nDetails: %s\nAudit: %s\nAffects routing: no\n' "$DETAILS" "$AUDIT"
