#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${DETAILS:=$WORK_ROOT/phi-default-instance-recovery-v7.jsonl}"

[[ -f "$DETAILS" ]] || { echo "missing PHI recovery details: $DETAILS" >&2; exit 2; }

python3 "$REPO_ROOT/tools/ml/summarize_nevergone_phi_default_instance_recovery.py" "$DETAILS"
