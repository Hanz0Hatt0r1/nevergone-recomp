#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

if [[ $# -ne 1 ]]; then
  echo "Usage: bash $0 /path/to/com.hippiegame.nevergone.apk" >&2
  exit 2
fi

ORIGINAL="$1"
DETAILS="$WORK_ROOT/controls-scheduler-action-callback-closure.jsonl"
AUDIT="$WORK_ROOT/controls-scheduler-action-callback-closure.txt"

for required in "$ORIGINAL" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_retained_alias_closure.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_scheduler_action_callbacks.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
mkdir -p "$WORK_ROOT"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_scheduler_action_callbacks.py" \
  "$ORIGINAL" --details-output "$DETAILS" | tee "$AUDIT"

printf '\nDetails: %s\nAudit: %s\nOriginal binary copied: no\nAffects routing: no\n' \
  "$DETAILS" "$AUDIT"
