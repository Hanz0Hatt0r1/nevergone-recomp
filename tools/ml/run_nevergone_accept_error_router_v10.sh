#!/usr/bin/env bash
set -euo pipefail

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
BASE_MODEL="$NGML/models/production-code-domain-v5.2"
BASE_OOF="$BASE_MODEL/oof_predictions.jsonl"
V8_OOF="$NGML/evidence-v8-monotone-correctness-routing/oof_monotone_correctness_routing.jsonl"
EVIDENCE="$NGML/evidence-v5.2/evidence.jsonl"
ROUTER_DIR="$NGML/models/accept-error-router-v10"
LOG_DIR="$NGML/logs"
mkdir -p "$ROUTER_DIR" "$LOG_DIR"

python3 tools/ml/train_nevergone_accept_error_router_v10.py \
  "$BASE_OOF" \
  "$V8_OOF" \
  "$ROUTER_DIR" \
  --evidence "$EVIDENCE"

run_optional() {
  local label="$1"
  local corpus="$2"
  local evidence="$3"
  [[ -n "$corpus" ]] || return 0
  [[ -f "$corpus" ]] || { echo "$label corpus missing: $corpus" >&2; return 2; }
  [[ -n "$evidence" && -f "$evidence" ]] || { echo "$label evidence missing: $evidence" >&2; return 2; }
  local output="$ROUTER_DIR/${label}.classified.jsonl"
  python3 tools/ml/classify_nevergone_pcode_with_accept_router.py \
    "$BASE_MODEL" \
    "$ROUTER_DIR" \
    "$corpus" \
    --evidence "$evidence" \
    --output "$output"
  python3 - "$label" "$output" <<'PY'
import collections, json, sys
label, path = sys.argv[1:]
rows = [json.loads(line) for line in open(path, encoding='utf-8') if line.strip()]
actions = collections.Counter(row.get('action') for row in rows)
base = collections.Counter(row.get('base_action') for row in rows)
changed = sum(bool(row.get('accept_router_changed')) for row in rows)
print(f'{label}: rows={len(rows)} base_actions={dict(base)} routed_actions={dict(actions)} changed={changed}')
PY
}

run_optional "unseen" "${V10_UNSEEN_CORPUS:-}" "${V10_UNSEEN_EVIDENCE:-}"
run_optional "ood" "${V10_OOD_CORPUS:-}" "${V10_OOD_EVIDENCE:-}"

echo
echo "== v10: production candidate =="
python3 - "$ROUTER_DIR/manifest.json" <<'PY'
import json, sys
p = sys.argv[1]
s = json.load(open(p, encoding='utf-8'))
print('format=', s['format'])
print('threshold=', s['threshold'])
print('crossfit accept precision=', round(s['calibration']['kept_precision'], 6))
print('crossfit accept coverage=', round(s['calibration']['kept_coverage'], 6))
print('legacy accept examples=', s['calibration']['legacy_accept_examples'])
print('kept examples=', s['calibration']['kept_examples'])
print('router=', s['artifact'])
PY
