#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
CONSUMER_AUDIT="$WORK_ROOT/gamesceneui-controls-return-consumer-audit.jsonl"
CALLER_AUDIT="$WORK_ROOT/caller-argument-subtree-audit.jsonl"
DETAILS="$WORK_ROOT/gamesceneui-controls-slot-convergence-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/gamesceneui-controls-slot-convergence-audit.txt"

for required in "$CATALOG" "$CONSUMER_AUDIT" "$CALLER_AUDIT"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

printf '== GameSceneUI / Controls slot convergence audit ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_gamesceneui_controls_slot_convergence.py" \
  "$CATALOG" "$CONSUMER_AUDIT" "$CALLER_AUDIT" \
  --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\ndetails: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
