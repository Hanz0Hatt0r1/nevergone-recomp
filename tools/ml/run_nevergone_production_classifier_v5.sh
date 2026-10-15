#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
RECEIVER_WORK_ROOT="${RECEIVER_WORK_ROOT:-$HOME/.cache/nevergone-recomp/receiver-provenance}"
DATASET="$NGML/dataset-4class"
FAMILIES="$NGML/families.jsonl"
EVIDENCE_DIR="$NGML/evidence-v5"
MODEL_DIR="$NGML/models/production-code-domain-v5"

for required in \
  "$DATASET/trace.jsonl" \
  "$FAMILIES" \
  "$NGML/corpus/libcocos2dcpp.jsonl" \
  "$NGML/corpus/libffmpeg.jsonl" \
  "docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz" \
  "docs/ghidra/full-index/libcocos2dcpp.calls.tsv.gz" \
  "docs/ghidra/full-index/libffmpeg.functions.tsv.gz" \
  "docs/ghidra/full-index/libffmpeg.calls.tsv.gz"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done

mkdir -p "$EVIDENCE_DIR" "$NGML/logs" "$NGML/models"

echo "== v5: callgraph evidence =="
python3 tools/ml/extract_nevergone_callgraph_metrics.py \
  "$NGML/corpus/libcocos2dcpp.jsonl" \
  docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz \
  docs/ghidra/full-index/libcocos2dcpp.calls.tsv.gz \
  --output "$EVIDENCE_DIR/libcocos2dcpp.callgraph.jsonl"

python3 tools/ml/extract_nevergone_callgraph_metrics.py \
  "$NGML/corpus/libffmpeg.jsonl" \
  docs/ghidra/full-index/libffmpeg.functions.tsv.gz \
  docs/ghidra/full-index/libffmpeg.calls.tsv.gz \
  --output "$EVIDENCE_DIR/libffmpeg.callgraph.jsonl"

receiver_args=()
if [[ -f "$RECEIVER_WORK_ROOT/indirect-receiver-types.tsv" ]]; then
  receiver_args+=(--receiver-types "$RECEIVER_WORK_ROOT/indirect-receiver-types.tsv")
fi

receiver_inference=""
for candidate in \
  static-receiver-vtable-inference-v6.jsonl \
  static-receiver-vtable-inference-v5.jsonl \
  static-receiver-vtable-inference-v4.jsonl \
  static-receiver-vtable-inference-v3.jsonl \
  static-receiver-vtable-inference-v2.jsonl; do
  if [[ -f "$RECEIVER_WORK_ROOT/$candidate" ]]; then
    receiver_inference="$RECEIVER_WORK_ROOT/$candidate"
    break
  fi
done
if [[ -n "$receiver_inference" ]]; then
  receiver_args+=(--receiver-inference "$receiver_inference")
fi

echo
echo "== v5: aggregate external evidence =="
python3 tools/ml/build_nevergone_v5_evidence.py \
  "$DATASET/trace.jsonl" \
  --callgraph-metrics "$EVIDENCE_DIR/libcocos2dcpp.callgraph.jsonl" \
  --callgraph-metrics "$EVIDENCE_DIR/libffmpeg.callgraph.jsonl" \
  "${receiver_args[@]}" \
  --output "$EVIDENCE_DIR/evidence.jsonl" \
  | tee "$NGML/logs/v5-evidence.log"

echo
echo "== v5: train =="
rm -rf "$MODEL_DIR"
python3 tools/ml/train_nevergone_production_classifier_v5.py \
  "$DATASET" \
  "$FAMILIES" \
  "$MODEL_DIR" \
  --evidence "$EVIDENCE_DIR/evidence.jsonl" \
  --classes game engine runtime_library media_library \
  | tee "$NGML/logs/v5-train.log"

echo
echo "== v5: comparison =="
python3 - \
  "$NGML/models/production-code-domain-v4/manifest.json" \
  "$MODEL_DIR/manifest.json" <<'PY'
import json
import sys
from pathlib import Path


def load(path):
    p = Path(path)
    if not p.exists():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


v4 = load(sys.argv[1])
v5 = load(sys.argv[2])
for name, payload in (("v4", v4), ("v5", v5)):
    if payload is None:
        print(name, "manifest: missing")
        continue
    metrics = payload["calibration"]["metrics"]
    selective = payload["routing"]["selective_oof"]["accept"]
    print(
        name,
        "balanced=", round(metrics["balanced_accuracy"], 4),
        "macro_f1=", round(metrics["macro_f1"], 4),
        "accept_precision=", round(selective["precision"], 4),
        "accept_coverage=", round(selective["coverage"], 4),
    )
PY

echo
echo "model: $MODEL_DIR"
echo "evidence: $EVIDENCE_DIR/evidence.jsonl"
