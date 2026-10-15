#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
BASE_OOF="$NGML/models/production-code-domain-v5.2/oof_predictions.jsonl"
META_OOF="$NGML/evidence-v5.6-game-meta-verifier/oof_meta_verifier_predictions.jsonl"
OUT_DIR="$NGML/evidence-v6-game-meta-routing"

for required in "$BASE_OOF" "$META_OOF"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done

mkdir -p "$OUT_DIR" "$NGML/logs"

echo "== v6: cross-fitted GAME meta routing =="
python3 tools/ml/evaluate_nevergone_game_meta_routing_v6.py \
  "$BASE_OOF" \
  "$META_OOF" \
  "$OUT_DIR" \
  --target-accept-precision 0.97 \
  --target-review-precision 0.80 \
  --max-accept-coverage-loss 0.04 \
  | tee "$NGML/logs/v6-game-meta-routing.log"

echo
echo "== v6: decision summary =="
python3 - "$OUT_DIR/summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
base = summary["base_actions"]
routed = summary["routed_actions"]
metrics = summary["class_metrics"]
print(
    "class metrics unchanged:",
    "balanced=", round(metrics["balanced_accuracy"], 4),
    "macro_f1=", round(metrics["macro_f1"], 4),
)
print(
    "base accept:",
    "precision=", round(base["accept"]["precision"], 4),
    "coverage=", round(base["accept"]["coverage"], 4),
    "examples=", base["accept"]["examples"],
)
print(
    "v6 accept:",
    "precision=", round(routed["accept"]["precision"], 4),
    "coverage=", round(routed["accept"]["coverage"], 4),
    "examples=", routed["accept"]["examples"],
)
print(
    "base review:",
    "precision=", round(base["review"]["precision"], 4),
    "coverage=", round(base["review"]["coverage"], 4),
)
print(
    "v6 review:",
    "precision=", round(routed["review"]["precision"], 4),
    "coverage=", round(routed["review"]["coverage"], 4),
)
print("selected thresholds:", summary["selected_thresholds"])
print("GAME accept:", summary["game_accept"])
print(
    "changed actions=", summary["changed_actions"],
    "changed-correct=", summary["changed_correct_predictions"],
    "changed-wrong=", summary["changed_incorrect_predictions"],
    "downgraded accepts=", summary["downgraded_accepts"],
)
PY

echo
echo "summary: $OUT_DIR/summary.json"
echo "rows: $OUT_DIR/oof_meta_routing_predictions.jsonl"
