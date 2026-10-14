#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"

PLAN="${1:-/tmp/nevergone-load-receiver-plan.tsv}"
INFERENCE="${2:-/tmp/nevergone-static-receiver-vtable-inference-v2.jsonl}"
CATALOG="${3:-}"
OUT="${4:-/tmp/nevergone-receiver-parameter-provenance.tsv}"
DETAILS="${5:-/tmp/nevergone-receiver-parameter-provenance-audit.jsonl}"
AUDIT_TXT="${6:-/tmp/nevergone-receiver-parameter-provenance-audit.txt}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$PLAN" ]] || { echo "missing plan: $PLAN" >&2; exit 2; }
[[ -f "$INFERENCE" ]] || { echo "missing inference: $INFERENCE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportIndirectCallReceiverParameterProvenance.java" ]] || {
  echo "missing parameter provenance exporter" >&2; exit 2;
}

if [[ -z "$CATALOG" ]]; then
  CATALOG="$(
    grep -rlE --include='*.jsonl' \
      '"format"[[:space:]]*:[[:space:]]*"NEVERGONE.StaticVtableSlotCatalog/3"' \
      /tmp "$REPO_ROOT" 2>/dev/null | head -n1 || true
  )"
fi
[[ -n "$CATALOG" && -f "$CATALOG" ]] || {
  echo "could not locate NEVERGONE.StaticVtableSlotCatalog/3; pass catalog as argument 3" >&2
  exit 2
}

echo "PLAN=$PLAN"
echo "INFERENCE=$INFERENCE"
echo "CATALOG=$CATALOG"
echo "OUT=$OUT"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallReceiverParameterProvenance.java "$PLAN" "$OUT"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_parameter_provenance.py" \
  "$INFERENCE" "$OUT" "$CATALOG" \
  --details-output "$DETAILS" --samples 60 | tee "$AUDIT_TXT"

echo "parameter provenance output: $OUT"
echo "audit details: $DETAILS"
echo "audit text: $AUDIT_TXT"
