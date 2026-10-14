#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${RECEIVER_TYPES:=/tmp/nevergone-indirect-receiver-types.tsv}"
: "${PARAMETER_PROVENANCE:=/tmp/nevergone-receiver-parameter-provenance.tsv}"
: "${BASELINE_INFERENCE:=/tmp/nevergone-static-receiver-vtable-inference-v2.jsonl}"
: "${OUTPUT:=/tmp/nevergone-static-receiver-vtable-inference-v3.jsonl}"
: "${QUERIES_OUTPUT:=/tmp/nevergone-static-receiver-vtable-queries-v3.jsonl}"
: "${FAILURE_DETAILS:=/tmp/nevergone-receiver-type-failures-v3.jsonl}"
: "${FAILURE_AUDIT:=/tmp/nevergone-receiver-type-failure-audit-v3.txt}"

find_jsonl_with_token() {
  local token="$1"
  local path
  for root in /tmp "$REPO_ROOT"; do
    [[ -d "$root" ]] || continue
    while IFS= read -r path; do
      if grep -Fqm1 "$token" "$path" 2>/dev/null; then
        printf '%s\n' "$path"
        return 0
      fi
    done < <(find "$root" -type f -name '*.jsonl' -print 2>/dev/null | sort)
  done
  return 1
}

if [[ -z "${ANALYSIS:-}" ]]; then
  ANALYSIS="$(find_jsonl_with_token '"indirect_call_slice_evidence"' || true)"
fi
if [[ -z "${CATALOG:-}" ]]; then
  CATALOG="$(find_jsonl_with_token '"NEVERGONE.StaticVtableSlotCatalog/3"' || true)"
fi

[[ -n "${ANALYSIS:-}" && -f "$ANALYSIS" ]] || {
  echo "missing analysis JSONL; set ANALYSIS=/path/to/analysis.jsonl" >&2
  exit 2
}
[[ -n "${CATALOG:-}" && -f "$CATALOG" ]] || {
  echo "missing static vtable catalog; set CATALOG=/path/to/catalog.jsonl" >&2
  exit 2
}
[[ -f "$RECEIVER_TYPES" ]] || { echo "missing receiver TSV: $RECEIVER_TYPES" >&2; exit 2; }
[[ -f "$PARAMETER_PROVENANCE" ]] || {
  echo "missing parameter provenance TSV: $PARAMETER_PROVENANCE" >&2
  echo "run tools/ml/run_nevergone_receiver_parameter_provenance.sh first" >&2
  exit 2
}

cmd=(
  python3 "$REPO_ROOT/tools/ml/infer_nevergone_static_receiver_vtables.py"
  "$ANALYSIS"
  "$RECEIVER_TYPES"
  "$CATALOG"
  --parameter-provenance "$PARAMETER_PROVENANCE"
  --output "$OUTPUT"
  --queries-output "$QUERIES_OUTPUT"
)

if [[ -n "${RUNTIME_OBSERVATIONS:-}" ]]; then
  [[ -f "$RUNTIME_OBSERVATIONS" ]] || {
    echo "missing runtime observations: $RUNTIME_OBSERVATIONS" >&2
    exit 2
  }
  cmd+=(--runtime-observations "$RUNTIME_OBSERVATIONS")
fi

printf 'analysis: %s\n' "$ANALYSIS"
printf 'catalog: %s\n' "$CATALOG"
printf 'receiver types: %s\n' "$RECEIVER_TYPES"
printf 'parameter provenance: %s\n' "$PARAMETER_PROVENANCE"
if [[ -n "${RUNTIME_OBSERVATIONS:-}" ]]; then
  printf 'runtime observations: %s\n' "$RUNTIME_OBSERVATIONS"
fi

"${cmd[@]}"

if [[ -f "$BASELINE_INFERENCE" ]]; then
  echo
  echo "baseline failure audit:"
  python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
    "$RECEIVER_TYPES" \
    "$BASELINE_INFERENCE" \
    --samples 0
fi

echo
echo "caller-aware failure audit:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" \
  "$OUTPUT" \
  --details-output "$FAILURE_DETAILS" \
  --samples 60 \
  | tee "$FAILURE_AUDIT"

printf 'inference output: %s\n' "$OUTPUT"
printf 'queries output: %s\n' "$QUERIES_OUTPUT"
printf 'failure details: %s\n' "$FAILURE_DETAILS"
printf 'failure audit: %s\n' "$FAILURE_AUDIT"
printf 'affects routing: no\n'
