#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
DEFAULT_REVENGINSTR="$(cd "$ROOT/.." && pwd)/revenginstr"
REVENGINSTR_REPO="${REVENGINSTR_REPO:-$DEFAULT_REVENGINSTR}"
BASE_OOF="$NGML/models/revenginstr-evidence-selective-v1/oof_predictions.jsonl"
EVIDENCE="$NGML/evidence-revenginstr-v5.2/evidence.jsonl"
REFERENCE="$NGML/models/accept-error-router-v10/manifest.json"
ROUTER="$NGML/models/revenginstr-accept-error-router-v1"
AUDIT="$NGML/evidence-revenginstr-selective/accept-router-gate.json"
LOG="$NGML/logs/revenginstr-accept-router-gate.txt"

for required in \
  "$BASE_OOF" \
  "$EVIDENCE" \
  "$REFERENCE" \
  "$REVENGINSTR_REPO/pyproject.toml" \
  "$REVENGINSTR_REPO/src/revenginstr/ml/accept_error_router.py"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done

mkdir -p "$NGML/models" "$(dirname "$AUDIT")" "$NGML/logs"
rm -rf "$ROUTER"
export PYTHONPATH="$REVENGINSTR_REPO/src${PYTHONPATH:+:$PYTHONPATH}"

echo "== revenginstr: package accept-error router from generic v5.2-equivalent OOF =="
python3 -c 'from revenginstr.ml.accept_error_router import train_main; raise SystemExit(train_main())' \
  "$BASE_OOF" \
  "$ROUTER" \
  --evidence "$EVIDENCE" \
  --seed 42 \
  --meta-folds 5 \
  --target-accept-precision 0.97 \
  --max-accept-coverage-loss 0.04 \
  --min-class-meta-examples 100 \
  --min-accept-examples 100 \
  | tee "$LOG"

echo
echo "== revenginstr: Nevergone production migration gate =="
python3 tools/ml/audit_nevergone_revenginstr_accept_router.py \
  "$REFERENCE" \
  "$ROUTER/manifest.json" \
  --min-precision 0.97 \
  --min-coverage 0.55 \
  --strict \
  --output "$AUDIT" \
  | tee -a "$LOG"

echo
echo "reference: $REFERENCE"
echo "candidate: $ROUTER"
echo "audit: $AUDIT"
