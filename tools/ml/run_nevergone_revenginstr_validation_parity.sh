#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
DEFAULT_REVENGINSTR="$(cd "$ROOT/.." && pwd)/revenginstr"
REVENGINSTR_REPO="${REVENGINSTR_REPO:-$DEFAULT_REVENGINSTR}"
HIST="$NGML/frozen-validation/unseen500-v10"
OUT="$NGML/frozen-validation/revenginstr-unseen500-v1"
INPUTS="$OUT/inputs"
NEW_FROZEN="$OUT/frozen"
LOG="$NGML/logs/revenginstr-validation-parity.txt"

WIDE="${NEVERGONE_VALIDATION_WIDE_CORPUS:-$HIST/wide-corpus.jsonl}"
DATASET_TRACE="${NEVERGONE_VALIDATION_DATASET_TRACE:-$NGML/dataset-4class/trace.jsonl}"
BASE_OOF="${NEVERGONE_VALIDATION_BASE_OOF:-$NGML/models/production-code-domain-v5.2/oof_predictions.jsonl}"
V9_OOF="${NEVERGONE_VALIDATION_ROUTING_OOF:-$NGML/evidence-v9-accept-error-routing/oof_accept_error_routing.jsonl}"
ROUTER_MANIFEST="${NEVERGONE_VALIDATION_ROUTER_MANIFEST:-$NGML/models/accept-error-router-v10/manifest.json}"
REFERENCE_EVIDENCE="${NEVERGONE_VALIDATION_REFERENCE_EVIDENCE:-$NGML/evidence-v5.2/evidence.jsonl}"
FROZEN_CLASSIFIED="${NEVERGONE_VALIDATION_FROZEN_CLASSIFIED:-$HIST/v10.classified.jsonl}"
FROZEN_EVIDENCE="${NEVERGONE_VALIDATION_FROZEN_EVIDENCE:-$HIST/evidence.jsonl}"
HISTORICAL_DRIFT="${NEVERGONE_VALIDATION_HISTORICAL_DRIFT:-$HIST/drift-summary.json}"

GENERIC_WIDE="$INPUTS/wide-corpus.revenginstr.jsonl"
ELIGIBILITY="$INPUTS/eligible-trace.jsonl"
REFERENCE_ACTIONS="$INPUTS/reference-base-actions.jsonl"
REFERENCE_EVIDENCE_GENERIC="$INPUTS/reference-evidence.revenginstr.jsonl"
FROZEN_EVIDENCE_GENERIC="$INPUTS/frozen-evidence.revenginstr.jsonl"
GENERIC_DRIFT="$OUT/historical-drift.revenginstr.json"
PARITY="$OUT/historical-drift-parity.json"

for required in \
  "$WIDE" \
  "$DATASET_TRACE" \
  "$BASE_OOF" \
  "$V9_OOF" \
  "$ROUTER_MANIFEST" \
  "$REFERENCE_EVIDENCE" \
  "$FROZEN_CLASSIFIED" \
  "$FROZEN_EVIDENCE" \
  "$HISTORICAL_DRIFT" \
  "$REVENGINSTR_REPO/pyproject.toml" \
  "$REVENGINSTR_REPO/src/revenginstr/validation/frozen.py" \
  "$REVENGINSTR_REPO/src/revenginstr/validation/drift.py"; do
  [[ -f "$required" ]] || { echo "missing: $required" >&2; exit 2; }
done

mkdir -p "$INPUTS" "$OUT" "$NGML/logs"
export PYTHONPATH="$REVENGINSTR_REPO/src${PYTHONPATH:+:$PYTHONPATH}"

python3 - <<'PY'
from revenginstr.validation import FROZEN_VALIDATION_FORMAT, DRIFT_AUDIT_FORMAT
from revenginstr.validation import drift
assert FROZEN_VALIDATION_FORMAT == "REVENGINSTR.FrozenValidation/1"
assert DRIFT_AUDIT_FORMAT == "REVENGINSTR.DriftAudit/1"
assert hasattr(drift, "overlay_reference_base_actions"), (
    "revenginstr is older than 0.5.2; run: cd ~/revenginstr && git pull --ff-only"
)
PY

{
  echo "== Nevergone -> revenginstr validation inputs =="
  python3 tools/ml/prepare_nevergone_revenginstr_validation_inputs.py \
    "$WIDE" \
    "$GENERIC_WIDE" \
    "$ELIGIBILITY" \
    --routing-oof "$V9_OOF" \
    --reference-actions-output "$REFERENCE_ACTIONS"

  echo
  echo "== Nevergone -> revenginstr evidence adapters =="
  python3 tools/ml/adapt_nevergone_evidence_to_revenginstr.py \
    "$REFERENCE_EVIDENCE" "$REFERENCE_EVIDENCE_GENERIC"
  python3 tools/ml/adapt_nevergone_evidence_to_revenginstr.py \
    "$FROZEN_EVIDENCE" "$FROZEN_EVIDENCE_GENERIC"

  echo
  echo "== revenginstr: freeze new deterministic Nevergone holdout generation =="
  if [[ -f "$NEW_FROZEN/manifest.json" ]]; then
    echo "reuse frozen manifest: $NEW_FROZEN/manifest.json"
    python3 - "$NEW_FROZEN/manifest.json" "$NEW_FROZEN/corpus.jsonl" "$NEW_FROZEN/trace.jsonl" <<'PY'
import hashlib, json, sys
manifest_path, corpus_path, trace_path = sys.argv[1:]
m = json.load(open(manifest_path, encoding="utf-8"))
def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()
assert m["format"] == "REVENGINSTR.FrozenValidation/1"
assert sha(corpus_path) == m["frozen_corpus"]["sha256"], "frozen corpus hash mismatch"
assert sha(trace_path) == m["trace"]["sha256"], "frozen trace hash mismatch"
print("frozen rows:", m["frozen_corpus"]["rows"])
print("corpus sha256:", m["frozen_corpus"]["sha256"])
PY
  else
    python3 -c 'from revenginstr.validation.frozen import main; raise SystemExit(main())' \
      "$GENERIC_WIDE" \
      "$NEW_FROZEN" \
      --eligible-trace "$ELIGIBILITY" \
      --exclude-trace "$DATASET_TRACE" \
      --limit 500 \
      --tag nevergone-revenginstr-unseen500-v1 \
      --selection-salt nevergone-revenginstr-validation-v1
  fi

  echo
  echo "== revenginstr: reproduce historical Nevergone frozen drift =="
  python3 -c 'from revenginstr.validation.drift import main; raise SystemExit(main())' \
    "$BASE_OOF" \
    "$FROZEN_CLASSIFIED" \
    --reference-base-actions "$REFERENCE_ACTIONS" \
    --reference-evidence "$REFERENCE_EVIDENCE_GENERIC" \
    --frozen-evidence "$FROZEN_EVIDENCE_GENERIC" \
    --router-manifest "$ROUTER_MANIFEST" \
    --output "$GENERIC_DRIFT"

  echo
  echo "== Nevergone historical -> revenginstr drift parity gate =="
  python3 tools/ml/audit_nevergone_revenginstr_validation_parity.py \
    "$HISTORICAL_DRIFT" \
    "$GENERIC_DRIFT" \
    --strict \
    --output "$PARITY"

  echo
  echo "== revenginstr validation migration outputs =="
  echo "new frozen generation: $NEW_FROZEN"
  echo "generic drift reproduction: $GENERIC_DRIFT"
  echo "parity audit: $PARITY"
} | tee "$LOG"
