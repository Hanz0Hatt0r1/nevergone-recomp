#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${FAILURES:=$WORK_ROOT/receiver-type-failures-v11.jsonl}"
: "${SLICE_DIR:=$WORK_ROOT/evidence/structural-receiver-slices}"
: "${PLAN:=$WORK_ROOT/load-address-root-probe-plan.tsv}"
: "${ROOT_CLASSIFICATION:=$WORK_ROOT/load-address-root-classification.tsv}"
: "${TARGET_CLASSIFICATION:=$WORK_ROOT/load-address-root-raw-target-classification.tsv}"
: "${DETAILS:=$WORK_ROOT/load-address-root-classification-audit.jsonl}"
: "${AUDIT_TXT:=$WORK_ROOT/load-address-root-classification-audit.txt}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
for required in "$FAILURES" "$REPO_ROOT/tools/ml/plan_nevergone_load_address_root_probe.py" "$REPO_ROOT/tools/ml/audit_nevergone_load_address_root_classification.py" "$REPO_ROOT/tools/ghidra/ExportAddressClassification.java"; do
  [[ -e "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -d "$SLICE_DIR" ]] || { echo "missing structural receiver slice directory: $SLICE_DIR" >&2; exit 2; }
mkdir -p "$WORK_ROOT"

python3 "$REPO_ROOT/tools/ml/plan_nevergone_load_address_root_probe.py" \
  "$FAILURES" "$SLICE_DIR" --output "$PLAN"

mapfile -t ROOTS < <(python3 - "$PLAN" <<'PY'
import csv, sys
with open(sys.argv[1], newline='', encoding='utf-8') as f:
    for row in csv.DictReader(f, delimiter='\t'):
        value = row.get('root_address')
        if value:
            print(value)
PY
)
[[ ${#ROOTS[@]} -gt 0 ]] || { echo "no LOAD address roots planned" >&2; exit 3; }

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java "$ROOT_CLASSIFICATION" "${ROOTS[@]}"

mapfile -t TARGETS < <(python3 - "$ROOT_CLASSIFICATION" <<'PY'
import csv, sys
seen=set()
with open(sys.argv[1], newline='', encoding='utf-8') as f:
    for row in csv.DictReader(f, delimiter='\t'):
        raw=(row.get('raw_u32') or '').strip()
        if raw and raw != '-':
            try: value=int(raw, 0)
            except ValueError: continue
            address=f"{value:08x}"
            if address not in seen:
                seen.add(address); print(address)
PY
)
[[ ${#TARGETS[@]} -gt 0 ]] || { echo "no raw u32 targets recovered from LOAD roots" >&2; exit 4; }

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java "$TARGET_CLASSIFICATION" "${TARGETS[@]}"

echo
echo "LOAD address-root classification audit:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_load_address_root_classification.py" \
  "$PLAN" "$ROOT_CLASSIFICATION" "$TARGET_CLASSIFICATION" \
  --details-output "$DETAILS" | tee "$AUDIT_TXT"

printf '\nplan: %s\n' "$PLAN"
printf 'root classification: %s\n' "$ROOT_CLASSIFICATION"
printf 'raw-target classification: %s\n' "$TARGET_CLASSIFICATION"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
