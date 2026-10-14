#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

FAILURES="$WORK_ROOT/receiver-type-failures-v12.jsonl"
PROVENANCE="$WORK_ROOT/structural-receiver-parameter-provenance.tsv"
SLICE_DIR="$WORK_ROOT/evidence/structural-receiver-slices"
SYMBOLS="$REPO_ROOT/docs/evidence/unidbg-2026-10-10/game.tsv"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
DETAILS="$WORK_ROOT/parameter-relative-receiver-field-audit.jsonl"
AUDIT="$WORK_ROOT/parameter-relative-receiver-field-audit.txt"

if [[ ! -f "$FAILURES" ]]; then
  echo "v12 failure audit missing; regenerate current v12 baseline"
  bash "$REPO_ROOT/tools/ml/run_nevergone_exact_singleton_cell_recovery_v12.sh"
fi

for required in "$FAILURES" "$PROVENANCE" "$SYMBOLS" "$CATALOG"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -d "$SLICE_DIR" ]] || { echo "missing structural slice dir: $SLICE_DIR" >&2; exit 2; }

python3 "$REPO_ROOT/tools/ml/audit_nevergone_parameter_relative_receiver_field.py" \
  "$FAILURES" "$PROVENANCE" "$SLICE_DIR" "$SYMBOLS" "$CATALOG" \
  --details-output "$DETAILS" | tee "$AUDIT"

printf '\ndetails: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT"
printf 'affects routing: no\n'
