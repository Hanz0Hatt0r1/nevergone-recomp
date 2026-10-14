#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${PLAN:=/tmp/nevergone-load-receiver-plan.tsv}"
: "${INFERENCE:=/tmp/nevergone-static-receiver-vtable-inference-v3.jsonl}"
: "${OUT:=/tmp/nevergone-structural-receiver-parameter-provenance.tsv}"
: "${DETAILS:=/tmp/nevergone-structural-receiver-parameter-provenance-audit.jsonl}"
: "${AUDIT_TXT:=/tmp/nevergone-structural-receiver-parameter-provenance-audit.txt}"

find_catalog() {
  local root path
  for root in /tmp "$REPO_ROOT"; do
    [[ -d "$root" ]] || continue
    while IFS= read -r path; do
      if grep -Fqm1 '"NEVERGONE.StaticVtableSlotCatalog/3"' "$path" 2>/dev/null; then
        printf '%s\n' "$path"
        return 0
      fi
    done < <(find "$root" -type f -name '*.jsonl' -print 2>/dev/null | sort)
  done
  return 1
}

if [[ -z "${CATALOG:-}" ]]; then
  CATALOG="$(find_catalog || true)"
fi

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$PLAN" ]] || { echo "missing plan: $PLAN" >&2; exit 2; }
[[ -f "$INFERENCE" ]] || { echo "missing caller-aware inference: $INFERENCE" >&2; exit 2; }
[[ -n "${CATALOG:-}" && -f "$CATALOG" ]] || { echo "missing StaticVtableSlotCatalog/3; set CATALOG=/path/to/catalog.jsonl" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportIndirectCallStructuralReceiverParameterProvenance.java" ]] || {
  echo "missing structural receiver parameter provenance exporter" >&2
  exit 2
}

printf 'plan: %s\n' "$PLAN"
printf 'inference: %s\n' "$INFERENCE"
printf 'catalog: %s\n' "$CATALOG"
printf 'output: %s\n' "$OUT"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallStructuralReceiverParameterProvenance.java "$PLAN" "$OUT"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_structural_receiver_parameter_provenance.py" \
  "$INFERENCE" \
  "$OUT" \
  "$CATALOG" \
  --details-output "$DETAILS" \
  --samples 60 \
  | tee "$AUDIT_TXT"

printf 'structural parameter provenance output: %s\n' "$OUT"
printf 'audit details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
