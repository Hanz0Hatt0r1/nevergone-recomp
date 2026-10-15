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
OUT_DIR="$NGML/evidence-v5.6-game-meta-verifier"

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

extra=()
if [[ -f "$CHAMPION" ]]; then
  extra+=(--champion-summary "$CHAMPION")
fi

echo "== v5.6: leakage-safe GAME meta verifier =="
python3 tools/ml/evaluate_nevergone_game_meta_verifier_v56.py \
  "$DATASET" \
  "$FAMILIES" \
  "$BASE_OOF" \
  "$OUT_DIR" \
  --evidence "$EVIDENCE" \
  "${extra[@]}" \
  | tee "$NGML/logs/v56-game-meta-verifier.log"

echo
echo "== v5.6: decision summary =="
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
    "v5.6-meta",
    "balanced=", round(meta["balanced_accuracy"], 4),
    "macro_f1=", round(meta["macro_f1"], 4),
    "game_precision=", round(meta["game_precision"], 4),
    "game_recall=", round(meta["game_recall"], 4),
    "game_f1=", round(meta["game_f1"], 4),
)
print("base boundary errors:", summary["base_boundary_errors"])
print("meta boundary errors:", summary["meta_boundary_errors"])
print(
    "flips=", meta["flips"],
    "helpful=", meta["helpful_flips"],
    "harmful=", meta["harmful_flips"],
)
print("evidence features=", summary["evidence_feature_count"])
print("meta features=", summary["meta_feature_count"])
print("v5.2 OOF reproduction rate=", round(summary["v52_oof_reproduction_rate"], 6))
PY

echo
echo "summary: $OUT_DIR/summary.json"
echo "rows: $OUT_DIR/oof_meta_verifier_predictions.jsonl"
