#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"

bash "$REPO_ROOT/tools/ml/run_nevergone_phi_secondary_load_tree_audit.sh"
printf '\n'
bash "$REPO_ROOT/tools/ml/run_nevergone_phi_secondary_resolved_address_classification.sh"
