#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${SLICE_DIR:=$WORK_ROOT/evidence/structural-receiver-slices}"
: "${ADDRESS_CLASSIFICATION:=$WORK_ROOT/phi-secondary-address-classification.tsv}"
: "${DETAILS:=$WORK_ROOT/phi-secondary-load-tree-audit.jsonl}"
: "${AUDIT_TXT:=$WORK_ROOT/phi-secondary-load-tree-audit.txt}"

[[ -d "$SLICE_DIR" ]] || { echo "missing structural receiver slice dir: $SLICE_DIR" >&2; exit 2; }
[[ -f "$ADDRESS_CLASSIFICATION" ]] || { echo "missing PHI address classification: $ADDRESS_CLASSIFICATION" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ml/audit_nevergone_phi_secondary_load_trees.py" ]] || { echo "missing PHI secondary LOAD tree audit" >&2; exit 2; }

printf '== PHI secondary LOAD tree audit ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_phi_secondary_load_trees.py" \
  "$SLICE_DIR" \
  "$ADDRESS_CLASSIFICATION" \
  --details-output "$DETAILS" \
  | tee "$AUDIT_TXT"

printf '\ndetails: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
