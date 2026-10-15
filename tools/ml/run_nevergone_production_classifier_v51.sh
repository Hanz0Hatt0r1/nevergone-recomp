#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
SCRIPT_PATH="$ROOT/tools/ghidra"
DATASET="$NGML/dataset-4class"
FAMILIES="$NGML/families.jsonl"
MASS_DIR="$NGML/receiver-v5.1"
EVIDENCE_DIR="$NGML/evidence-v5.1"
MODEL_DIR="$NGML/models/production-code-domain-v5.1"
PLAN="$MASS_DIR/receiver-plan.tsv"
VTABLE_SLOTS="$MASS_DIR/vtable-slots.tsv.gz"
CATALOG="$MASS_DIR/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$MASS_DIR/indirect-receiver-types.tsv"
RECEIVER_INFERENCE="$MASS_DIR/static-receiver-vtable-inference-mass.jsonl"

for required in \
  "$DATASET/trace.jsonl" \
  "$FAMILIES" \
  "$NGML/corpus/libcocos2dcpp.jsonl" \
  "$NGML/corpus/libffmpeg.jsonl" \
  "docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz" \
  "docs/ghidra/full-index/libcocos2dcpp.calls.tsv.gz" \
  "docs/ghidra/full-index/libffmpeg.functions.tsv.gz" \
  "docs/ghidra/full-index/libffmpeg.calls.tsv.gz" \
  "$SCRIPT_PATH/ExportVtableSlots.java" \
  "$SCRIPT_PATH/ExportIndirectCallReceiverTypes.java"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }

mkdir -p "$MASS_DIR" "$EVIDENCE_DIR" "$NGML/logs" "$NGML/models"

echo "== v5.1: build mass receiver plan =="
python3 tools/ml/build_nevergone_mass_receiver_plan.py \
  "$DATASET/trace.jsonl" \
  "$PLAN" \
  --program "$GHIDRA_PROGRAM" \
  | tee "$NGML/logs/v51-receiver-plan.log"

echo
echo "== v5.1: mass Ghidra receiver/type export =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$SCRIPT_PATH" \
  -postScript ExportVtableSlots.java "$VTABLE_SLOTS" \
  -postScript ExportIndirectCallReceiverTypes.java "$PLAN" "$RECEIVER_TYPES" \
  2>&1 | tee "$NGML/logs/v51-receiver-export.log"

echo
echo "== v5.1: build static vtable catalog =="
python3 tools/ml/build_nevergone_static_vtable_slot_catalog_from_export.py \
  "$VTABLE_SLOTS" \
  --output "$CATALOG" \
  | tee "$NGML/logs/v51-vtable-catalog.log"

echo
echo "== v5.1: mass receiver identity/vtable inference =="
python3 tools/ml/infer_nevergone_mass_receiver_vtables.py \
  "$RECEIVER_TYPES" \
  "$CATALOG" \
  --output "$RECEIVER_INFERENCE" \
  | tee "$NGML/logs/v51-receiver-inference.log"

echo
echo "== v5.1: callgraph evidence =="
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

echo
echo "== v5.1: evidence coverage audit =="
python3 tools/ml/audit_nevergone_v51_evidence_coverage.py \
  "$DATASET/trace.jsonl" \
  --callgraph-metrics "$EVIDENCE_DIR/libcocos2dcpp.callgraph.jsonl" \
  --callgraph-metrics "$EVIDENCE_DIR/libffmpeg.callgraph.jsonl" \
  --receiver-types "$RECEIVER_TYPES" \
  --receiver-inference "$RECEIVER_INFERENCE" \
  --receiver-program "$GHIDRA_PROGRAM" \
  | tee "$NGML/logs/v51-evidence-coverage.log"

echo
echo "== v5.1: aggregate external evidence =="
python3 tools/ml/build_nevergone_v5_evidence.py \
  "$DATASET/trace.jsonl" \
  --callgraph-metrics "$EVIDENCE_DIR/libcocos2dcpp.callgraph.jsonl" \
  --callgraph-metrics "$EVIDENCE_DIR/libffmpeg.callgraph.jsonl" \
  --receiver-types "$RECEIVER_TYPES" \
  --receiver-inference "$RECEIVER_INFERENCE" \
  --output "$EVIDENCE_DIR/evidence.jsonl" \
  | tee "$NGML/logs/v51-evidence.log"

echo
echo "== v5.1: train =="
rm -rf "$MODEL_DIR"
python3 tools/ml/train_nevergone_production_classifier_v5.py \
  "$DATASET" \
  "$FAMILIES" \
  "$MODEL_DIR" \
  --evidence "$EVIDENCE_DIR/evidence.jsonl" \
  --classes game engine runtime_library media_library \
  | tee "$NGML/logs/v51-train.log"

echo
echo "== v5.1: comparison =="
python3 - \
  "$NGML/models/production-code-domain-v4/manifest.json" \
  "$NGML/models/production-code-domain-v5/manifest.json" \
  "$MODEL_DIR/manifest.json" <<'PY'
import json
import sys
from pathlib import Path


def load(path):
    p = Path(path)
    if not p.exists():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


for name, path in zip(("v4", "v5", "v5.1"), sys.argv[1:]):
    payload = load(path)
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
echo "receiver types: $RECEIVER_TYPES"
echo "receiver inference: $RECEIVER_INFERENCE"
echo "evidence: $EVIDENCE_DIR/evidence.jsonl"
echo "model: $MODEL_DIR"
