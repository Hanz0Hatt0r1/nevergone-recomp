#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
DEFAULT_REVENGINSTR="$(cd "$ROOT/.." && pwd)/revenginstr"
REVENGINSTR_REPO="${REVENGINSTR_REPO:-$DEFAULT_REVENGINSTR}"
DATASET="$NGML/dataset-4class"
FAMILIES="$NGML/families.jsonl"
REFERENCE="$NGML/models/production-code-domain-v4"
CANDIDATE="$NGML/models/revenginstr-selective-v1"
PARITY="$NGML/evidence-revenginstr-selective/parity.json"
LOG="$NGML/logs/revenginstr-selective-parity.txt"

for required in \
  "$DATASET/train.jsonl" \
  "$DATASET/validation.jsonl" \
  "$DATASET/test.jsonl" \
  "$FAMILIES" \
  "$REFERENCE/manifest.json" \
  "$REFERENCE/oof_predictions.jsonl" \
  "$REVENGINSTR_REPO/pyproject.toml" \
  "$REVENGINSTR_REPO/src/revenginstr/ml/selective.py"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done

mkdir -p "$NGML/models" "$(dirname "$PARITY")" "$NGML/logs"
rm -rf "$CANDIDATE"

export PYTHONPATH="$REVENGINSTR_REPO/src${PYTHONPATH:+:$PYTHONPATH}"

echo "== revenginstr selective: train on frozen Nevergone dataset =="
python3 -c 'from revenginstr.ml.selective import train_main; raise SystemExit(train_main())' \
  "$DATASET" \
  "$FAMILIES" \
  "$CANDIDATE" \
  --task code_domain \
  --classes game engine runtime_library media_library \
  --folds 5 \
  --seed 42 \
  --accept-precision 0.95 \
  --review-precision 0.85 \
  --min-class-calibration-examples 25 \
  --min-class-threshold-examples 10 \
  --min-ood-threshold-examples 10 \
  | tee "$LOG"

echo
echo "== revenginstr selective: strict historical v4 parity =="
python3 tools/ml/audit_nevergone_revenginstr_selective_parity.py \
  "$REFERENCE/oof_predictions.jsonl" \
  "$CANDIDATE/oof_predictions.jsonl" \
  "$REFERENCE/manifest.json" \
  "$CANDIDATE/manifest.json" \
  --tolerance 1e-10 \
  --strict \
  --output "$PARITY" \
  | tee -a "$LOG"

echo
echo "reference: $REFERENCE"
echo "candidate: $CANDIDATE"
echo "parity: $PARITY"
