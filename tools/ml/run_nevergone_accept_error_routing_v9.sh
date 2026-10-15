#!/usr/bin/env bash
set -euo pipefail

NGML="${1:-$HOME/.local/share/nevergone-ml-v2}"
if (($#)); then
  shift
fi
BASE_OOF="$NGML/models/production-code-domain-v5.2/oof_predictions.jsonl"
V8_OOF="$NGML/evidence-v8-monotone-correctness-routing/oof_monotone_correctness_routing.jsonl"
EVIDENCE="$NGML/evidence-v5.2/evidence.jsonl"
OUT="$NGML/evidence-v9-accept-error-routing"
LOG_DIR="$NGML/logs"
mkdir -p "$OUT" "$LOG_DIR"

python3 tools/ml/evaluate_nevergone_accept_error_routing_v9.py \
  "$BASE_OOF" \
  "$V8_OOF" \
  "$OUT" \
  --evidence "$EVIDENCE" \
  "$@"

python3 - "$OUT/summary.json" <<'PY'
import json, sys
p = sys.argv[1]
s = json.load(open(p, encoding='utf-8'))
print()
print('== v9: decision summary ==')
for key, label in [('legacy_routing','legacy'), ('v9_routing','v9')]:
    a = s[key]['accept']
    r = s[key]['review']
    print(label, 'accept: precision=', round(a['precision'],4), 'coverage=', round(a['coverage'],4), 'examples=', a['examples'])
    print(label, 'review: precision=', round(r['precision'],4), 'coverage=', round(r['coverage'],4))
print('selected thresholds:', s['selected_thresholds'])
print('promotions=', s['promotions'])
print('changed actions=', s['changed_actions'], 'downgraded-wrong=', s['downgraded_wrong'], 'downgraded-correct=', s['downgraded_correct'])
print('legacy accept attribution:', s['legacy_accept_attribution'])
print('v9 accept attribution:', s['v9_accept_attribution'])
print('summary:', p)
print('rows:', s['rows_path'])
PY
