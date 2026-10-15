#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
DATASET="$NGML/dataset-4class"
FAMILIES="$NGML/families.jsonl"
EVIDENCE="$NGML/evidence-v5.2/evidence.jsonl"
BASE_OOF="$NGML/models/production-code-domain-v5.2/oof_predictions.jsonl"
OUT_DIR="$NGML/evidence-v7-correctness-meta-routing"

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

echo "== v7: nested all-class correctness meta routing =="
python3 tools/ml/evaluate_nevergone_correctness_meta_routing_v7.py \
  "$DATASET" \
  "$FAMILIES" \
  "$BASE_OOF" \
  "$OUT_DIR" \
  --evidence "$EVIDENCE" \
  | tee "$NGML/logs/v7-correctness-meta-routing.log"

echo
echo "== v7: decision summary =="
python3 - "$OUT_DIR/summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
metrics = summary["class_metrics"]
legacy = summary["legacy_routing"]
meta = summary["meta_routing"]
print(
    "class metrics unchanged:",
    "balanced=", round(metrics["balanced_accuracy"], 4),
    "macro_f1=", round(metrics["macro_f1"], 4),
)
print(
    "legacy accept:",
    "precision=", round(legacy["accept"]["precision"], 4),
    "coverage=", round(legacy["accept"]["coverage"], 4),
    "examples=", legacy["accept"]["examples"],
)
print(
    "v7 accept:",
    "precision=", round(meta["accept"]["precision"], 4),
    "coverage=", round(meta["accept"]["coverage"], 4),
    "examples=", meta["accept"]["examples"],
)
print(
    "legacy review:",
    "precision=", round(legacy["review"]["precision"], 4),
    "coverage=", round(legacy["review"]["coverage"], 4),
)
print(
    "v7 review:",
    "precision=", round(meta["review"]["precision"], 4),
    "coverage=", round(meta["review"]["coverage"], 4),
)
print("selected thresholds:", summary["selected_thresholds"])
print("legacy accept attribution:", summary["legacy_accept_attribution"])
print("v7 accept attribution:", summary["meta_accept_attribution"])
print("v5.2 OOF reproduction rate=", round(summary["v52_oof_reproduction_rate"], 6))
PY

echo
echo "summary: $OUT_DIR/summary.json"
echo "rows: $OUT_DIR/oof_correctness_meta_routing.jsonl"
