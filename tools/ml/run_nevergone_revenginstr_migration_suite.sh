#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
DEFAULT_REVENGINSTR="$(cd "$ROOT/.." && pwd)/revenginstr"
export REVENGINSTR_REPO="${REVENGINSTR_REPO:-$DEFAULT_REVENGINSTR}"

[[ -d "$REVENGINSTR_REPO/src/revenginstr" ]] || {
  echo "missing revenginstr checkout: $REVENGINSTR_REPO" >&2
  exit 2
}

echo "== revenginstr migration suite: P-code-only v4 parity =="
bash tools/ml/run_nevergone_revenginstr_selective_parity.sh "$NGML"

echo
echo "== revenginstr migration suite: evidence-aware v5.2 parity =="
bash tools/ml/run_nevergone_revenginstr_evidence_parity.sh "$NGML"

echo
echo "== revenginstr migration suite: accept-router production gate =="
bash tools/ml/run_nevergone_revenginstr_accept_router_gate.sh "$NGML"

echo
echo "== revenginstr migration suite: PASS =="
echo "P-code parity: $NGML/evidence-revenginstr-selective/parity.json"
echo "Evidence parity: $NGML/evidence-revenginstr-selective/evidence-v5.2-parity.json"
echo "Accept router gate: $NGML/evidence-revenginstr-selective/accept-router-gate.json"
