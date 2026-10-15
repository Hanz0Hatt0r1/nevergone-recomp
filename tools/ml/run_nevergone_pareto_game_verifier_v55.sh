#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
DATASET="$NGML/dataset-4class"
FAMILIES="$NGML/families.jsonl"
EVIDENCE="$NGML/evidence-v5.2/evidence.jsonl"
BASE_OOF="$NGML/models/production-code-domain-v5.2/oof_predictions.jsonl"
OUT_DIR="$NGML/evidence-v5.5-pareto-game-verifier"

for required in \
  "$DATASET/train.jsonl" \
  "$DATASET/validation.jsonl" \
  "$DATASET/test.jsonl" \
  "$FAMILIES" \
  "$EVIDENCE" \
  "$BASE_OOF"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done

mkdir -p "$OUT_DIR" "$NGML/logs"

echo "== v5.5: nested Pareto game verifier =="
python3 tools/ml/evaluate_nevergone_pareto_game_verifier_v55.py \
  "$DATASET" \
  "$FAMILIES" \
  "$BASE_OOF" \
  "$OUT_DIR" \
  --evidence "$EVIDENCE" \
  | tee "$NGML/logs/v55-pareto-game-verifier.log"

echo
echo "== v5.5: decision summary =="
python3 - "$OUT_DIR/summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
base = summary["base_metrics"]
verified = summary["verified_metrics"]
print(
    "base-v5.2",
    "balanced=", round(base["balanced_accuracy"], 4),
    "macro_f1=", round(base["macro_f1"], 4),
    "game_precision=", round(summary["base_game"]["precision"], 4),
    "game_recall=", round(summary["base_game"]["recall"], 4),
    "game_f1=", round(summary["base_game"]["f1"], 4),
)
print(
    "v5.5-pareto",
    "balanced=", round(verified["balanced_accuracy"], 4),
    "macro_f1=", round(verified["macro_f1"], 4),
    "game_precision=", round(summary["verified_game"]["precision"], 4),
    "game_recall=", round(summary["verified_game"]["recall"], 4),
    "game_f1=", round(summary["verified_game"]["f1"], 4),
)
print("base boundary errors:", summary["base_boundary_errors"])
print("pareto boundary errors:", summary["verified_boundary_errors"])
print(
    "flips=", summary["flips"],
    "helpful=", summary["helpful_flips"],
    "harmful=", summary["harmful_flips"],
)
print("policy family rows:", summary["policy_family_rows"])
print("v5.2 OOF reproduction rate=", round(summary["reference_v52_reproduction"]["rate"], 6))
PY

echo
echo "summary: $OUT_DIR/summary.json"
echo "rows: $OUT_DIR/oof_pareto_verifier_predictions.jsonl"
