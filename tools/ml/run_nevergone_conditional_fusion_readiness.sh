#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "Usage: bash $0 DATASET_DIR FAMILIES_JSONL [OUTPUT_DIR]" >&2
  exit 2
fi

DATASET_DIR="$1"
FAMILIES="$2"
OUT_DIR="${3:-/tmp/nevergone-conditional-fusion-readiness}"
: "${REPO_ROOT:=$PWD}"
: "${OUTER_FOLDS:=5}"
: "${INNER_FOLDS:=4}"
: "${SEED:=42}"
: "${MIN_OUTER_PRECISION:=0.95}"
: "${MIN_INNER_PRECISION:=0.95}"
: "${MIN_PROMOTIONS_PER_POLICY:=10}"
: "${MIN_ENABLED_FOLDS:=3}"
: "${MIN_TOTAL_PROMOTIONS:=10}"
: "${REQUIRE_READY:=0}"

CV_LOG="$OUT_DIR/conditional-fusion-code-domain.log"
READINESS="$OUT_DIR/conditional-fusion-readiness.json"

for required in \
  "$REPO_ROOT/tools/ml/nevergone_nested_conditional_fusion_cv.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_conditional_fusion_readiness.py"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done
[[ -d "$DATASET_DIR" ]] || { echo "missing dataset directory: $DATASET_DIR" >&2; exit 2; }
[[ -f "$FAMILIES" ]] || { echo "missing family map: $FAMILIES" >&2; exit 2; }
mkdir -p "$OUT_DIR"

# Game is deliberately not a trusted transfer class in this stage. The purpose
# is to test whether independent family geometry can safely recover conservative
# engine/media/runtime review or escalate rows without weakening game routing.
python3 "$REPO_ROOT/tools/ml/nevergone_nested_conditional_fusion_cv.py" \
  "$DATASET_DIR" "$FAMILIES" \
  --task code_domain \
  --classes game engine runtime_library media_library \
  --trusted-transfer-classes engine media_library runtime_library \
  --outer-folds "$OUTER_FOLDS" \
  --inner-folds "$INNER_FOLDS" \
  --seed "$SEED" \
  --accept-precision 0.95 \
  --review-precision 0.85 \
  --transfer-precision 0.95 \
  --conditional-precision 0.95 \
  | tee "$CV_LOG"

gate_args=(
  "$CV_LOG"
  --output "$READINESS"
  --scope per_label_action
  --expected-folds "$OUTER_FOLDS"
  --candidate-labels engine media_library runtime_library
  --min-outer-precision "$MIN_OUTER_PRECISION"
  --min-examples "$MIN_PROMOTIONS_PER_POLICY"
  --min-enabled-folds "$MIN_ENABLED_FOLDS"
  --min-inner-precision "$MIN_INNER_PRECISION"
  --min-total-promotions "$MIN_TOTAL_PROMOTIONS"
)
if [[ "$REQUIRE_READY" == "1" ]]; then
  gate_args+=(--require-ready)
elif [[ "$REQUIRE_READY" != "0" ]]; then
  echo "REQUIRE_READY must be 0 or 1" >&2
  exit 2
fi

python3 "$REPO_ROOT/tools/ml/audit_nevergone_conditional_fusion_readiness.py" \
  "${gate_args[@]}"

python3 - "$READINESS" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text(encoding="utf-8"))
if data.get("production_routing_changed") is not False:
    raise SystemExit("readiness artifact unexpectedly changes production routing")
print()
print("readiness:", data["ready_for_independent_validation"])
print("eligible policy keys:", data["eligible_policy_keys"])
print("candidate fused accept:", data["candidate_fused_accept"])
print("next gate: independent validation before any production routing change")
PY

printf '\nlog: %s\nreadiness artifact: %s\nproduction routing changed: no\n' \
  "$CV_LOG" "$READINESS"
