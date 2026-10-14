#!/usr/bin/env bash
set -euo pipefail
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/com.hippiegame.nevergone.apk (or extracted libcocos2dcpp.so)" >&2
  exit 2
fi

ORIGINAL="$1"
INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v11.jsonl"
SLICES="$WORK_ROOT/evidence/mixed-callind-receiver-002a7594.tsv"
DETAILS="$WORK_ROOT/controlsbutton-field-receiver-witness.jsonl"
AUDIT="$WORK_ROOT/controlsbutton-field-receiver-witness.txt"

for required in "$ORIGINAL" "$INFERENCE" "$SLICES" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controlsbutton_field_receiver_witness.py"; do
  [[ -f "$required" ]] || { echo "missing input: $required" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/audit_nevergone_controlsbutton_field_receiver_witness.py" \
  "$ORIGINAL" "$INFERENCE" "$SLICES" --details-output "$DETAILS" | tee "$AUDIT"

printf '\nDetails: %s\nAudit: %s\nOriginal binary copied: no\nAffects routing: no\n' \
  "$DETAILS" "$AUDIT"
