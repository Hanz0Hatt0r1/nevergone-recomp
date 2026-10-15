#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
DATASET="$NGML/dataset-4class"
FAMILIES="$NGML/families.jsonl"
EVIDENCE="$NGML/evidence-v5.2/evidence.jsonl"
BASE_OOF="$NGML/models/production-code-domain-v5.2/oof_predictions.jsonl"
CHAMPION="$NGML/evidence-v5.4-selective-game-verifier/summary.json"
OUT_DIR="$NGML/evidence-v5.7-constrained-game-meta"

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

args=(
  "$DATASET"
  "$FAMILIES"
  "$BASE_OOF"
  "$OUT_DIR"
  --evidence "$EVIDENCE"
)
[[ -f "$CHAMPION" ]] && args+=(--champion-summary "$CHAMPION")

echo "== v5.7: nested constrained GAME meta calibration =="
python3 tools/ml/evaluate_nevergone_constrained_game_meta_v57.py "${args[@]}" \
  | tee "$NGML/logs/v57-constrained-game-meta.log"

echo
echo "== v5.7: decision summary =="
python3 - "$OUT_DIR/summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
base = summary["base"]
meta = summary["meta"]
print(
    "base-v5.2",
    "balanced=", round(base["balanced_accuracy"], 4),
    "macro_f1=", round(base["macro_f1"], 4),
    "game_precision=", round(base["game_precision"], 4),
    "game_recall=", round(base["game_recall"], 4),
    "game_f1=", round(base["game_f1"], 4),
)
print(
    "v5.7-constrained-meta",
    "balanced=", round(meta["balanced_accuracy"], 4),
    "macro_f1=", round(meta["macro_f1"], 4),
    "game_precision=", round(meta["game_precision"], 4),
    "game_recall=", round(meta["game_recall"], 4),
    "game_f1=", round(meta["game_f1"], 4),
)
print("selected thresholds:", [round(x, 4) for x in summary["selected_thresholds"]])
print("base boundary errors:", summary["base_boundary_errors"])
print("meta boundary errors:", summary["meta_boundary_errors"])
print(
    "flips=", meta["flips"],
    "helpful=", meta["helpful_flips"],
    "harmful=", meta["harmful_flips"],
)
print("v5.2 OOF reproduction rate=", round(summary["v52_oof_reproduction_rate"], 6))
PY

echo
echo "summary: $OUT_DIR/summary.json"
echo "rows: $OUT_DIR/oof_constrained_meta_predictions.jsonl"
