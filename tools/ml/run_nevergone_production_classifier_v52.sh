#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
DATASET="$NGML/dataset-4class"
FAMILIES="$NGML/families.jsonl"
RECEIVER_DIR="$NGML/receiver-v5.1"
V51_EVIDENCE="$NGML/evidence-v5.1"
EVIDENCE_DIR="$NGML/evidence-v5.2"
MODEL_DIR="$NGML/models/production-code-domain-v5.2"
BASE_EVIDENCE="$EVIDENCE_DIR/evidence-base.jsonl"
EVIDENCE="$EVIDENCE_DIR/evidence.jsonl"
BOUNDARY_ERRORS="$EVIDENCE_DIR/game-engine-boundary-errors.jsonl"
BOUNDARY_SUMMARY="$EVIDENCE_DIR/game-engine-boundary-summary.json"
RECEIVER_TYPES="$RECEIVER_DIR/indirect-receiver-types.tsv"
RECEIVER_INFERENCE="$RECEIVER_DIR/static-receiver-vtable-inference-mass.jsonl"
CG_COCO="$EVIDENCE_DIR/libcocos2dcpp.callgraph.jsonl"
CG_FFMPEG="$EVIDENCE_DIR/libffmpeg.callgraph.jsonl"

for required in \
  "$DATASET/trace.jsonl" \
  "$FAMILIES" \
  "$NGML/corpus/libcocos2dcpp.jsonl" \
  "$NGML/corpus/libffmpeg.jsonl" \
  "$RECEIVER_TYPES" \
  "$RECEIVER_INFERENCE" \
  "docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz" \
  "docs/ghidra/full-index/libcocos2dcpp.calls.tsv.gz" \
  "docs/ghidra/full-index/libffmpeg.functions.tsv.gz" \
  "docs/ghidra/full-index/libffmpeg.calls.tsv.gz"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done

mkdir -p "$EVIDENCE_DIR" "$NGML/logs" "$NGML/models"

echo "== v5.2: callgraph evidence =="
if [[ -f "$V51_EVIDENCE/libcocos2dcpp.callgraph.jsonl" ]]; then
  cp "$V51_EVIDENCE/libcocos2dcpp.callgraph.jsonl" "$CG_COCO"
else
  python3 tools/ml/extract_nevergone_callgraph_metrics.py \
    "$NGML/corpus/libcocos2dcpp.jsonl" \
    docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz \
    docs/ghidra/full-index/libcocos2dcpp.calls.tsv.gz \
    --output "$CG_COCO"
fi

if [[ -f "$V51_EVIDENCE/libffmpeg.callgraph.jsonl" ]]; then
  cp "$V51_EVIDENCE/libffmpeg.callgraph.jsonl" "$CG_FFMPEG"
else
  python3 tools/ml/extract_nevergone_callgraph_metrics.py \
    "$NGML/corpus/libffmpeg.jsonl" \
    docs/ghidra/full-index/libffmpeg.functions.tsv.gz \
    docs/ghidra/full-index/libffmpeg.calls.tsv.gz \
    --output "$CG_FFMPEG"
fi

echo
echo "== v5.2: aggregate count evidence =="
python3 tools/ml/build_nevergone_v5_evidence.py \
  "$DATASET/trace.jsonl" \
  --callgraph-metrics "$CG_COCO" \
  --callgraph-metrics "$CG_FFMPEG" \
  --receiver-types "$RECEIVER_TYPES" \
  --receiver-inference "$RECEIVER_INFERENCE" \
  --output "$BASE_EVIDENCE" \
  | tee "$NGML/logs/v52-evidence-base.log"

echo
echo "== v5.2: receiver ratio augmentation =="
python3 tools/ml/augment_nevergone_v52_receiver_ratios.py \
  "$BASE_EVIDENCE" \
  --output "$EVIDENCE" \
  | tee "$NGML/logs/v52-ratios.log"

echo
echo "== v5.2: train =="
rm -rf "$MODEL_DIR"
python3 tools/ml/train_nevergone_production_classifier_v5.py \
  "$DATASET" \
  "$FAMILIES" \
  "$MODEL_DIR" \
  --evidence "$EVIDENCE" \
  --classes game engine runtime_library media_library \
  | tee "$NGML/logs/v52-train.log"

echo
echo "== v5.2: game/engine boundary audit =="
python3 tools/ml/audit_nevergone_game_engine_boundary.py \
  "$MODEL_DIR/oof_predictions.jsonl" \
  "$EVIDENCE" \
  --output "$BOUNDARY_ERRORS" \
  --summary-output "$BOUNDARY_SUMMARY" \
  | tee "$NGML/logs/v52-game-engine-boundary.log"

echo
echo "== v5.2: comparison =="
python3 - \
  "$NGML/models/production-code-domain-v4/manifest.json" \
  "$NGML/models/production-code-domain-v5/manifest.json" \
  "$NGML/models/production-code-domain-v5.1/manifest.json" \
  "$MODEL_DIR/manifest.json" <<'PY'
import json
import sys
from pathlib import Path


def load(path):
    p = Path(path)
    if not p.exists():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


for name, path in zip(("v4", "v5", "v5.1", "v5.2"), sys.argv[1:]):
    payload = load(path)
    if payload is None:
        print(name, "manifest: missing")
        continue
    metrics = payload["calibration"]["metrics"]
    selective = payload["routing"]["selective_oof"]["accept"]
    game = payload["routing"]["class_policies"].get("game", {})
    print(
        name,
        "balanced=", round(metrics["balanced_accuracy"], 4),
        "macro_f1=", round(metrics["macro_f1"], 4),
        "accept_precision=", round(selective["precision"], 4),
        "accept_coverage=", round(selective["coverage"], 4),
        "game_precision=", round(float(game.get("oof_precision", 0.0)), 4),
    )
PY

optional_eval() {
  local label="$1"
  local corpus="$2"
  local evidence_path="$3"
  local output="$EVIDENCE_DIR/${label}.classified.jsonl"
  [[ -n "$corpus" ]] || return 0
  [[ -f "$corpus" ]] || { echo "${label} corpus missing: $corpus" >&2; return 2; }
  local args=(python3 tools/ml/classify_nevergone_pcode.py "$MODEL_DIR" "$corpus" --output "$output")
  if [[ -n "$evidence_path" ]]; then
    [[ -f "$evidence_path" ]] || { echo "${label} evidence missing: $evidence_path" >&2; return 2; }
    args+=(--evidence "$evidence_path")
  fi
  echo
  echo "== v5.2: optional ${label} inference =="
  "${args[@]}"
  echo "${label} output: $output"
}

optional_eval "unseen" "${V52_UNSEEN_CORPUS:-}" "${V52_UNSEEN_EVIDENCE:-}"
optional_eval "ood" "${V52_OOD_CORPUS:-}" "${V52_OOD_EVIDENCE:-}"

echo
echo "evidence: $EVIDENCE"
echo "model: $MODEL_DIR"
echo "boundary errors: $BOUNDARY_ERRORS"
echo "boundary summary: $BOUNDARY_SUMMARY"
