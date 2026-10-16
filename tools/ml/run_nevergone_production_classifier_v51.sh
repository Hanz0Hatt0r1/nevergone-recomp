#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"

DEFAULT_REVENGINSTR="$(cd "$ROOT/.." && pwd)/revenginstr"
REVENGINSTR_REPO=${REVENGINSTR_REPO:-}
if [[ -z "$REVENGINSTR_REPO" && -f "$DEFAULT_REVENGINSTR/pyproject.toml" ]]; then
  REVENGINSTR_REPO="$DEFAULT_REVENGINSTR"
fi
USE_REVENGINSTR=0
if [[ -n "$REVENGINSTR_REPO" \
   && -f "$REVENGINSTR_REPO/src/revenginstr/evidence/callgraph.py" \
   && -f "$REVENGINSTR_REPO/src/revenginstr/evidence/aggregate.py" \
   && -f "$REVENGINSTR_REPO/src/revenginstr/receiver/plan.py" \
   && -f "$REVENGINSTR_REPO/src/revenginstr/receiver/infer.py" ]]; then
  USE_REVENGINSTR=1
  export PYTHONPATH="$REVENGINSTR_REPO/src${PYTHONPATH:+:$PYTHONPATH}"
fi

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
REVENGINSTR_RECEIVER_INFERENCE="$MASS_DIR/revenginstr-receiver-inference.jsonl"
RECEIVER_INFERENCE="$MASS_DIR/static-receiver-vtable-inference-mass.jsonl"
REVENGINSTR_EVIDENCE="$EVIDENCE_DIR/revenginstr-evidence.jsonl"
EVIDENCE="$EVIDENCE_DIR/evidence.jsonl"

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

if [[ "$USE_REVENGINSTR" == "1" ]]; then
  echo "== generic evidence core: revenginstr =="
  echo "revenginstr: $REVENGINSTR_REPO"
else
  echo "== generic evidence core: Nevergone compatibility fallback =="
  echo "warning: clone/update revenginstr next to this repo or set REVENGINSTR_REPO" >&2
fi

echo
echo "== v5.1: build mass receiver plan =="
if [[ "$USE_REVENGINSTR" == "1" ]]; then
  python3 -m revenginstr.receiver.plan \
    "$DATASET/trace.jsonl" "$PLAN" --program "$GHIDRA_PROGRAM" \
    | tee "$NGML/logs/v51-receiver-plan.log"
else
  python3 tools/ml/build_nevergone_mass_receiver_plan.py \
    "$DATASET/trace.jsonl" "$PLAN" --program "$GHIDRA_PROGRAM" \
    | tee "$NGML/logs/v51-receiver-plan.log"
fi

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
  "$VTABLE_SLOTS" --output "$CATALOG" \
  | tee "$NGML/logs/v51-vtable-catalog.log"

echo
echo "== v5.1: mass receiver identity/vtable inference =="
if [[ "$USE_REVENGINSTR" == "1" ]]; then
  python3 -m revenginstr.receiver.infer \
    "$RECEIVER_TYPES" "$CATALOG" \
    --output "$REVENGINSTR_RECEIVER_INFERENCE" \
    | tee "$NGML/logs/v51-revenginstr-receiver-inference.log"
  python3 tools/ml/adapt_revenginstr_receiver_inference.py \
    "$REVENGINSTR_RECEIVER_INFERENCE" \
    --output "$RECEIVER_INFERENCE" \
    | tee "$NGML/logs/v51-receiver-inference.log"
else
  python3 tools/ml/infer_nevergone_mass_receiver_vtables.py \
    "$RECEIVER_TYPES" "$CATALOG" --output "$RECEIVER_INFERENCE" \
    | tee "$NGML/logs/v51-receiver-inference.log"
fi

echo
echo "== v5.1: callgraph evidence =="
if [[ "$USE_REVENGINSTR" == "1" ]]; then
  python3 -m revenginstr.evidence.callgraph \
    "$NGML/corpus/libcocos2dcpp.jsonl" \
    docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz \
    docs/ghidra/full-index/libcocos2dcpp.calls.tsv.gz \
    --output "$EVIDENCE_DIR/libcocos2dcpp.callgraph.jsonl"
  python3 -m revenginstr.evidence.callgraph \
    "$NGML/corpus/libffmpeg.jsonl" \
    docs/ghidra/full-index/libffmpeg.functions.tsv.gz \
    docs/ghidra/full-index/libffmpeg.calls.tsv.gz \
    --output "$EVIDENCE_DIR/libffmpeg.callgraph.jsonl"
else
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
fi

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
if [[ "$USE_REVENGINSTR" == "1" ]]; then
  python3 -m revenginstr.evidence.aggregate \
    "$DATASET/trace.jsonl" \
    --callgraph-metrics "$EVIDENCE_DIR/libcocos2dcpp.callgraph.jsonl" \
    --callgraph-metrics "$EVIDENCE_DIR/libffmpeg.callgraph.jsonl" \
    --receiver-types "$RECEIVER_TYPES" \
    --receiver-inference "$REVENGINSTR_RECEIVER_INFERENCE" \
    --output "$REVENGINSTR_EVIDENCE" \
    | tee "$NGML/logs/v51-revenginstr-evidence.log"
  python3 tools/ml/adapt_revenginstr_evidence.py \
    "$REVENGINSTR_EVIDENCE" --output "$EVIDENCE" \
    | tee "$NGML/logs/v51-evidence.log"
else
  python3 tools/ml/build_nevergone_v5_evidence.py \
    "$DATASET/trace.jsonl" \
    --callgraph-metrics "$EVIDENCE_DIR/libcocos2dcpp.callgraph.jsonl" \
    --callgraph-metrics "$EVIDENCE_DIR/libffmpeg.callgraph.jsonl" \
    --receiver-types "$RECEIVER_TYPES" \
    --receiver-inference "$RECEIVER_INFERENCE" \
    --output "$EVIDENCE" \
    | tee "$NGML/logs/v51-evidence.log"
fi

echo
echo "== v5.1: train =="
rm -rf "$MODEL_DIR"
python3 tools/ml/train_nevergone_production_classifier_v5.py \
  "$DATASET" "$FAMILIES" "$MODEL_DIR" \
  --evidence "$EVIDENCE" \
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
if [[ -f "$REVENGINSTR_RECEIVER_INFERENCE" ]]; then
  echo "generic receiver inference: $REVENGINSTR_RECEIVER_INFERENCE"
fi
echo "receiver inference compatibility output: $RECEIVER_INFERENCE"
if [[ -f "$REVENGINSTR_EVIDENCE" ]]; then
  echo "generic evidence: $REVENGINSTR_EVIDENCE"
fi
echo "evidence compatibility output: $EVIDENCE"
echo "model: $MODEL_DIR"
