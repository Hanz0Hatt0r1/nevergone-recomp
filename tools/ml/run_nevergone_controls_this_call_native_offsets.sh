#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

if [[ $# -ne 2 ]]; then
  echo "Usage: bash $0 /path/to/nevergone-controls-field-alias-evidence.tar.gz /path/to/com.hippiegame.nevergone.apk" >&2
  exit 2
fi

EVIDENCE="$1"
ORIGINAL="$2"
TRIAGE="$WORK_ROOT/controls-field-alias-triage.jsonl"
OLD_PLAN="$WORK_ROOT/controls-field-next-probe-plan.tsv"
DETAILS="$WORK_ROOT/controls-this-call-native-offsets.jsonl"
AUDIT="$WORK_ROOT/controls-this-call-native-offsets.txt"

for required in "$EVIDENCE" "$ORIGINAL" \
  "$REPO_ROOT/tools/ml/triage_nevergone_controls_field_alias_evidence.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_this_call_native_offsets.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
mkdir -p "$WORK_ROOT"

python3 "$REPO_ROOT/tools/ml/triage_nevergone_controls_field_alias_evidence.py" \
  "$EVIDENCE" "$ORIGINAL" \
  --details-output "$TRIAGE" \
  --plan-output "$OLD_PLAN"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_this_call_native_offsets.py" \
  "$TRIAGE" "$ORIGINAL" --details-output "$DETAILS" | tee "$AUDIT"

printf '\nDetails: %s\nAudit: %s\nOriginal binary copied: no\nAffects routing: no\n' \
  "$DETAILS" "$AUDIT"
