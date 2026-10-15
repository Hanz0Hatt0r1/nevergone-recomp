#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 4 || $# -gt 5 ]]; then
  echo "Usage: bash $0 READINESS_JSON PRODUCTION_CLASSIFIER_DIR DATASET_DIR FAMILIES_JSONL [OUTPUT_DIR]" >&2
  exit 2
fi

READINESS="$1"
PRODUCTION_DIR="$2"
DATASET_DIR="$3"
FAMILIES="$4"
OUT_DIR="${5:-/tmp/nevergone-conditional-fusion-candidate}"
: "${REPO_ROOT:=$PWD}"
: "${TRANSFER_FOLDS:=5}"
: "${TRANSFER_SEED:=42}"
: "${TRANSFER_PRECISION:=0.95}"
: "${CONDITIONAL_PRECISION:=0.95}"
: "${MIN_TRANSFER_EXAMPLES:=10}"
: "${MIN_CONDITIONAL_EXAMPLES:=10}"

PRODUCTION_MANIFEST="$PRODUCTION_DIR/manifest.json"
CLASSIFIER_OOF="$PRODUCTION_DIR/oof_predictions.jsonl"
TRANSFER_DIR="$OUT_DIR/transfer-oof"
CANDIDATE="$OUT_DIR/conditional-fusion-candidate.json"

for required in \
  "$READINESS" \
  "$PRODUCTION_MANIFEST" \
  "$CLASSIFIER_OOF" \
  "$FAMILIES" \
  "$REPO_ROOT/tools/ml/export_nevergone_similarity_transfer_oof.py" \
  "$REPO_ROOT/tools/ml/build_nevergone_conditional_fusion_candidate.py"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done
[[ -d "$DATASET_DIR" ]] || { echo "missing dataset directory: $DATASET_DIR" >&2; exit 2; }
mkdir -p "$OUT_DIR"

mapfile -t META < <(python3 - "$PRODUCTION_MANIFEST" "$READINESS" <<'PY'
import json
import sys
production = json.load(open(sys.argv[1], encoding="utf-8"))
readiness = json.load(open(sys.argv[2], encoding="utf-8"))
if production.get("format") != "NEVERGONE.ProductionClassifier/4":
    raise SystemExit("production manifest must be v4")
if readiness.get("format") != "NEVERGONE.ConditionalFusionReadiness/1":
    raise SystemExit("readiness format mismatch")
if readiness.get("ready_for_independent_validation") is not True:
    raise SystemExit("readiness gate did not pass")
if production.get("task") != readiness.get("task"):
    raise SystemExit("task mismatch")
classes = production.get("classes")
if not isinstance(classes, list) or not classes or not all(isinstance(x, str) for x in classes):
    raise SystemExit("production manifest has invalid classes")
print(production["task"])
for value in classes:
    print(value)
PY
)
[[ ${#META[@]} -ge 2 ]] || { echo "failed to read production task/classes" >&2; exit 2; }
TASK="${META[0]}"
CLASSES=("${META[@]:1}")

rm -rf "$TRANSFER_DIR"
python3 "$REPO_ROOT/tools/ml/export_nevergone_similarity_transfer_oof.py" \
  "$DATASET_DIR" "$FAMILIES" "$TRANSFER_DIR" \
  --task "$TASK" \
  --classes "${CLASSES[@]}" \
  --folds "$TRANSFER_FOLDS" \
  --seed "$TRANSFER_SEED"

python3 "$REPO_ROOT/tools/ml/build_nevergone_conditional_fusion_candidate.py" \
  "$READINESS" \
  "$PRODUCTION_MANIFEST" \
  "$CLASSIFIER_OOF" \
  "$TRANSFER_DIR/manifest.json" \
  "$TRANSFER_DIR/transfer_oof_predictions.jsonl" \
  --output "$CANDIDATE" \
  --transfer-precision "$TRANSFER_PRECISION" \
  --conditional-precision "$CONDITIONAL_PRECISION" \
  --min-transfer-examples "$MIN_TRANSFER_EXAMPLES" \
  --min-conditional-examples "$MIN_CONDITIONAL_EXAMPLES"

python3 - "$CANDIDATE" <<'PY'
import json
import sys
candidate = json.load(open(sys.argv[1], encoding="utf-8"))
if candidate.get("validation_state") != "pending_independent_validation":
    raise SystemExit("candidate validation state mismatch")
if candidate.get("production_routing_changed") is not False:
    raise SystemExit("candidate unexpectedly changes production routing")
print()
print("frozen eligible keys:", candidate["eligible_policy_keys"])
print("development OOF:", candidate["development_oof"])
print("next gate: independent labelled export; do not enable production routing yet")
PY

printf '\ncandidate: %s\ntransfer OOF: %s\nproduction routing changed: no\n' \
  "$CANDIDATE" "$TRANSFER_DIR"
