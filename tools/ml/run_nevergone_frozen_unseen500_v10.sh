#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${V10_FROZEN_LIMIT:=500}"
: "${V10_FROZEN_EXPORT_LIMIT:=2000}"
: "${V10_FROZEN_EXPORT_FALLBACK:=4000}"
: "${V10_REFRESH_FROZEN:=0}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
SCRIPT_PATH="$ROOT/tools/ghidra"
TRAINING_DATASET="${V10_FROZEN_TRAINING_DATASET:-$NGML/dataset-4class}"
BASE_MODEL="$NGML/models/production-code-domain-v5.2"
BASE_OOF="$BASE_MODEL/oof_predictions.jsonl"
V9_OOF="$NGML/evidence-v9-accept-error-routing/oof_accept_error_routing.jsonl"
TRAIN_EVIDENCE="$NGML/evidence-v5.2/evidence.jsonl"
ROUTER_DIR="$NGML/models/accept-error-router-v10"
ROUTER_MANIFEST="$ROUTER_DIR/manifest.json"
OUT="$NGML/frozen-validation/unseen500-v10"
SOURCE="$OUT/wide-corpus.jsonl"
CORPUS="$OUT/corpus.jsonl"
TRACE="$OUT/trace.jsonl"
FREEZE_MANIFEST="$OUT/manifest.json"
PLAN="$OUT/receiver-plan.tsv"
RECEIVER_TYPES="$OUT/indirect-receiver-types.tsv"
RECEIVER_INFERENCE="$OUT/static-receiver-vtable-inference.jsonl"
CALLGRAPH="$OUT/callgraph.jsonl"
EVIDENCE_BASE="$OUT/evidence-base.jsonl"
EVIDENCE="$OUT/evidence.jsonl"
CLASSIFIED="$OUT/v10.classified.jsonl"
SUMMARY="$OUT/validation-summary.json"
DRIFT_SUMMARY="$OUT/drift-summary.json"
VTABLE_SLOTS="$NGML/receiver-v5.1/vtable-slots.tsv.gz"
VTABLE_CATALOG="$NGML/receiver-v5.1/static-vtable-slot-catalog.jsonl"
LOG_DIR="$NGML/logs"
REVENGINSTR_REPO="${REVENGINSTR_REPO:-$ROOT/../revenginstr}"
SHARED_PCODE_RUNNER="$REVENGINSTR_REPO/scripts/run_ghidra_pcode_export.sh"
LEGACY_PCODE_EXPORTER="$SCRIPT_PATH/ExportMlPcodeCorpus.java"

for required in \
  "$TRAINING_DATASET/train.jsonl" \
  "$TRAINING_DATASET/validation.jsonl" \
  "$TRAINING_DATASET/test.jsonl" \
  "$BASE_MODEL/classifier.joblib" \
  "$BASE_OOF" \
  "$V9_OOF" \
  "$TRAIN_EVIDENCE" \
  "$ROUTER_DIR/accept_error_router.joblib" \
  "$ROUTER_MANIFEST" \
  "docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz" \
  "docs/ghidra/full-index/libcocos2dcpp.calls.tsv.gz" \
  "$SCRIPT_PATH/ExportIndirectCallReceiverTypes.java"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
if [[ ! -f "$SHARED_PCODE_RUNNER" && ! -f "$LEGACY_PCODE_EXPORTER" ]]; then
  echo "missing shared and legacy P-code exporters" >&2
  exit 2
fi

mkdir -p "$OUT" "$LOG_DIR"

freeze_export() {
  local max_functions="$1"
  echo "== frozen unseen: export up to $max_functions functions =="
  rm -f "$SOURCE"
  if [[ -f "$SHARED_PCODE_RUNNER" ]]; then
    echo "using revenginstr P-code exporter: $SHARED_PCODE_RUNNER"
    GHIDRA_HOME="$GHIDRA_HOME" bash "$SHARED_PCODE_RUNNER" \
      "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" "$GHIDRA_PROGRAM" \
      "$SOURCE" "$max_functions" \
      2>&1 | tee "$LOG_DIR/v10-frozen-unseen-export.log"
  else
    echo "using legacy local P-code exporter"
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" \
      -recursive \
      -readOnly \
      -noanalysis \
      -scriptPath "$SCRIPT_PATH" \
      -postScript ExportMlPcodeCorpus.java "$SOURCE" "$max_functions" \
      2>&1 | tee "$LOG_DIR/v10-frozen-unseen-export.log"
  fi
}

if [[ "$V10_REFRESH_FROZEN" == "1" ]]; then
  rm -f "$FREEZE_MANIFEST" "$CORPUS" "$TRACE"
fi

if [[ ! -f "$FREEZE_MANIFEST" ]]; then
  freeze_export "$V10_FROZEN_EXPORT_LIMIT"
  if ! python3 tools/ml/freeze_nevergone_unseen_corpus.py \
      "$SOURCE" "$TRAINING_DATASET" "$OUT" \
      --limit "$V10_FROZEN_LIMIT" \
      --tag unseen500-v10; then
    echo "initial export did not provide enough unseen internal rows; retrying with $V10_FROZEN_EXPORT_FALLBACK" >&2
    rm -f "$FREEZE_MANIFEST" "$CORPUS" "$TRACE"
    freeze_export "$V10_FROZEN_EXPORT_FALLBACK"
    python3 tools/ml/freeze_nevergone_unseen_corpus.py \
      "$SOURCE" "$TRAINING_DATASET" "$OUT" \
      --limit "$V10_FROZEN_LIMIT" \
      --tag unseen500-v10
  fi
else
  echo "== frozen unseen: reuse immutable corpus =="
  python3 - "$FREEZE_MANIFEST" "$CORPUS" "$TRACE" <<'PY'
import hashlib, json, sys
manifest_path, corpus_path, trace_path = sys.argv[1:]
m = json.load(open(manifest_path, encoding='utf-8'))
def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()
assert sha(corpus_path) == m['frozen_corpus']['sha256'], 'frozen corpus hash mismatch'
assert sha(trace_path) == m['trace']['sha256'], 'frozen trace hash mismatch'
print('frozen rows:', m['frozen_corpus']['rows'])
print('corpus sha256:', m['frozen_corpus']['sha256'])
PY
fi

echo
echo "== frozen unseen: callgraph evidence =="
python3 tools/ml/extract_nevergone_callgraph_metrics.py \
  "$CORPUS" \
  docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz \
  docs/ghidra/full-index/libcocos2dcpp.calls.tsv.gz \
  --output "$CALLGRAPH"

echo
echo "== frozen unseen: receiver plan =="
python3 tools/ml/build_nevergone_mass_receiver_plan.py \
  "$TRACE" "$PLAN" --program "$GHIDRA_PROGRAM"

if [[ ! -f "$VTABLE_CATALOG" ]]; then
  echo
  echo "== frozen unseen: rebuild vtable catalog =="
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" \
    -recursive \
    -readOnly \
    -noanalysis \
    -scriptPath "$SCRIPT_PATH" \
    -postScript ExportVtableSlots.java "$VTABLE_SLOTS" \
    2>&1 | tee "$LOG_DIR/v10-frozen-unseen-vtables.log"
  python3 tools/ml/build_nevergone_static_vtable_slot_catalog_from_export.py \
    "$VTABLE_SLOTS" --output "$VTABLE_CATALOG"
fi

echo
echo "== frozen unseen: receiver/type evidence =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$SCRIPT_PATH" \
  -postScript ExportIndirectCallReceiverTypes.java "$PLAN" "$RECEIVER_TYPES" \
  2>&1 | tee "$LOG_DIR/v10-frozen-unseen-receivers.log"

python3 tools/ml/infer_nevergone_mass_receiver_vtables.py \
  "$RECEIVER_TYPES" "$VTABLE_CATALOG" \
  --output "$RECEIVER_INFERENCE"

echo
echo "== frozen unseen: aggregate v5.2 evidence =="
python3 tools/ml/build_nevergone_v5_evidence.py \
  "$TRACE" \
  --callgraph-metrics "$CALLGRAPH" \
  --receiver-types "$RECEIVER_TYPES" \
  --receiver-inference "$RECEIVER_INFERENCE" \
  --output "$EVIDENCE_BASE"

python3 tools/ml/augment_nevergone_v52_receiver_ratios.py \
  "$EVIDENCE_BASE" --output "$EVIDENCE"

echo
echo "== frozen unseen: v10 routed inference =="
python3 tools/ml/classify_nevergone_pcode_with_accept_router.py \
  "$BASE_MODEL" "$ROUTER_DIR" "$CORPUS" \
  --evidence "$EVIDENCE" \
  --output "$CLASSIFIED"

python3 - "$FREEZE_MANIFEST" "$EVIDENCE" "$CLASSIFIED" "$SUMMARY" <<'PY'
import collections, hashlib, json, sys
manifest_path, evidence_path, classified_path, summary_path = sys.argv[1:]
manifest = json.load(open(manifest_path, encoding='utf-8'))
evidence = [json.loads(line) for line in open(evidence_path, encoding='utf-8') if line.strip()]
rows = [json.loads(line) for line in open(classified_path, encoding='utf-8') if line.strip()]

def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()

actions = collections.Counter(row.get('action') for row in rows)
base_actions = collections.Counter(row.get('base_action') for row in rows)
preds = collections.Counter(row.get('prediction') for row in rows)
changed = sum(bool(row.get('accept_router_changed')) for row in rows)
features = [row.get('features', {}) for row in evidence]
with_external = sum(bool(f) for f in features)
with_receiver = sum(bool(f.get('has_receiver_types') or f.get('has_receiver_inference')) for f in features)
with_callgraph = sum(bool(f.get('has_callgraph')) for f in features)
summary = {
    'format': 'NEVERGONE.FrozenValidationV10/1',
    'frozen_tag': manifest['tag'],
    'rows': len(rows),
    'corpus_sha256': manifest['frozen_corpus']['sha256'],
    'classified_sha256': sha(classified_path),
    'evidence_sha256': sha(evidence_path),
    'prediction_counts': dict(sorted(preds.items())),
    'base_action_counts': dict(sorted(base_actions.items())),
    'routed_action_counts': dict(sorted(actions.items())),
    'router_changed': changed,
    'router_changed_fraction': changed / len(rows) if rows else 0.0,
    'evidence': {
        'rows': len(evidence),
        'with_any_external': with_external,
        'with_callgraph': with_callgraph,
        'with_receiver': with_receiver,
    },
    'labels_available': False,
    'note': 'Identity-disjoint unlabeled holdout: validates deployment behavior/evidence coverage, not semantic precision.',
}
open(summary_path,'w',encoding='utf-8').write(json.dumps(summary, indent=2, sort_keys=True)+'\n')
print()
print('== v10 frozen unseen500 summary ==')
print('rows=', summary['rows'])
print('corpus_sha256=', summary['corpus_sha256'])
print('predictions=', summary['prediction_counts'])
print('base_actions=', summary['base_action_counts'])
print('routed_actions=', summary['routed_action_counts'])
print('router_changed=', changed, 'fraction=', round(summary['router_changed_fraction'], 4))
print('evidence=', summary['evidence'])
print('labels_available=', summary['labels_available'])
print('summary=', summary_path)
print('classified=', classified_path)
PY

echo
echo "== frozen unseen: distribution-shift audit =="
python3 tools/ml/audit_nevergone_frozen_drift_v10.py \
  "$BASE_OOF" \
  "$V9_OOF" \
  "$ROUTER_MANIFEST" \
  "$TRAIN_EVIDENCE" \
  "$CLASSIFIED" \
  "$EVIDENCE" \
  --output "$DRIFT_SUMMARY"
