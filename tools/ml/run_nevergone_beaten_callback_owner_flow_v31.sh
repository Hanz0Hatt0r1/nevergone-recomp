#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

PCODE="$WORK_ROOT/evidence/protobuf-nested-recovery-v25"
V29="$WORK_ROOT/beaten-callback-alias-audit-v29.json"
V30="$WORK_ROOT/beaten-callback-selectors-audit-v30.json"
V27="$WORK_ROOT/beaten-loop-writer-audit-v27.json"
OUT="$WORK_ROOT/beaten-callback-owner-flow-audit-v31.json"
ARCH="$HOME/nevergone-beaten-callback-owner-flow-v31.tar.gz"

for input in "$PCODE/pcode-002a3a20.tsv" "$PCODE/pcode-002a3c74.tsv" "$V29" "$V30" "$V27"; do
  [[ -f "$input" ]] || { echo "missing required input: $input" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_callback_owner_flow_v31.py" \
  "$PCODE" "$V29" "$V30" "$V27" --output "$OUT"
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-callback-owner-flow-audit-v31.json \
  beaten-callback-alias-audit-v29.json \
  beaten-callback-selectors-audit-v30.json \
  beaten-loop-writer-audit-v27.json \
  evidence/protobuf-nested-recovery-v25/pcode-002a3a20.tsv \
  evidence/protobuf-nested-recovery-v25/pcode-002a3c74.tsv
printf 'archive: %s\n' "$ARCH"
