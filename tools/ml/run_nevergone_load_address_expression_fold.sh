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
: "${ROOT_CLASSIFICATION:=$WORK_ROOT/load-address-root-classification.tsv}"
: "${RAW_TARGET_CLASSIFICATION:=$WORK_ROOT/load-address-root-raw-target-classification.tsv}"
: "${FOLD_DETAILS:=$WORK_ROOT/load-address-expression-fold-audit.jsonl}"
: "${FOLD_AUDIT_TXT:=$WORK_ROOT/load-address-expression-fold-audit.txt}"
: "${EFFECTIVE_CLASSIFICATION:=$WORK_ROOT/load-effective-address-classification.tsv}"
: "${VALUE_CLASSIFICATION:=$WORK_ROOT/load-effective-value-classification.tsv}"
: "${REFERENCE_OWNERS:=$WORK_ROOT/load-effective-address-reference-owners.tsv}"
: "${OWNER_DETAILS:=$WORK_ROOT/load-effective-address-owner-audit.jsonl}"
: "${OWNER_AUDIT_TXT:=$WORK_ROOT/load-effective-address-owner-audit.txt}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
for required in \
  "$FAILURES" "$ROOT_CLASSIFICATION" "$RAW_TARGET_CLASSIFICATION" \
  "$REPO_ROOT/tools/ml/audit_nevergone_load_address_expression_fold.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_load_effective_address_owners.py" \
  "$REPO_ROOT/tools/ghidra/ExportAddressClassification.java" \
  "$REPO_ROOT/tools/ghidra/ExportAddressReferenceOwners.java"; do
  [[ -e "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -d "$SLICE_DIR" ]] || { echo "missing structural receiver slice directory: $SLICE_DIR" >&2; exit 2; }
mkdir -p "$WORK_ROOT"

echo "== LOAD address expression fold =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_load_address_expression_fold.py" \
  "$FAILURES" "$SLICE_DIR" "$ROOT_CLASSIFICATION" "$RAW_TARGET_CLASSIFICATION" \
  --details-output "$FOLD_DETAILS" | tee "$FOLD_AUDIT_TXT"

mapfile -t EFFECTIVE_ADDRESSES < <(python3 - "$FOLD_DETAILS" <<'PY'
import json, sys
seen=set()
with open(sys.argv[1], encoding='utf-8') as f:
    for raw in f:
        if not raw.strip():
            continue
        row=json.loads(raw)
        value=row.get('effective_address')
        if row.get('status') == 'effective_address_folded' and isinstance(value, int):
            text=f"{value & 0xffffffff:08x}"
            if text not in seen:
                seen.add(text); print(text)
PY
)
[[ ${#EFFECTIVE_ADDRESSES[@]} -gt 0 ]] || { echo "no effective LOAD addresses folded" >&2; exit 3; }

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java "$EFFECTIVE_CLASSIFICATION" "${EFFECTIVE_ADDRESSES[@]}"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressReferenceOwners.java "$REFERENCE_OWNERS" "${EFFECTIVE_ADDRESSES[@]}"

mapfile -t RAW_VALUES < <(python3 - "$EFFECTIVE_CLASSIFICATION" <<'PY'
import csv, sys
seen=set()
with open(sys.argv[1], newline='', encoding='utf-8') as f:
    for row in csv.DictReader(f, delimiter='\t'):
        raw=(row.get('raw_u32') or '').strip()
        if not raw or raw == '-':
            continue
        try:
            value=int(raw, 0) & 0xffffffff
        except ValueError:
            continue
        text=f"{value:08x}"
        if text not in seen:
            seen.add(text); print(text)
PY
)
[[ ${#RAW_VALUES[@]} -gt 0 ]] || { echo "no raw values recovered from folded effective addresses" >&2; exit 4; }

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java "$VALUE_CLASSIFICATION" "${RAW_VALUES[@]}"

echo
echo "== LOAD effective address owner audit =="
python3 "$REPO_ROOT/tools/ml/audit_nevergone_load_effective_address_owners.py" \
  "$FOLD_DETAILS" "$EFFECTIVE_CLASSIFICATION" "$VALUE_CLASSIFICATION" "$REFERENCE_OWNERS" \
  --details-output "$OWNER_DETAILS" | tee "$OWNER_AUDIT_TXT"

printf '\nfold details: %s\n' "$FOLD_DETAILS"
printf 'fold audit: %s\n' "$FOLD_AUDIT_TXT"
printf 'effective classification: %s\n' "$EFFECTIVE_CLASSIFICATION"
printf 'value classification: %s\n' "$VALUE_CLASSIFICATION"
printf 'reference owners: %s\n' "$REFERENCE_OWNERS"
printf 'owner details: %s\n' "$OWNER_DETAILS"
printf 'owner audit: %s\n' "$OWNER_AUDIT_TXT"
printf 'affects routing: no\n'
