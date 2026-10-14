#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
OWNER_AUDIT="$WORK_ROOT/parameter-relative-receiver-field-audit.jsonl"
FIELD_STORES="$WORK_ROOT/ccjoystick-init-field-factory-stores.tsv"
FACTORY_PLAN="$WORK_ROOT/ccjoystick-field-factory-return-plan.tsv"
FACTORY_PROV="$WORK_ROOT/ccjoystick-field-factory-return-provenance.tsv"
FACTORY_AUDIT="$WORK_ROOT/ccjoystick-field-factory-return-audit.jsonl"
FACTORY_AUDIT_TXT="$WORK_ROOT/ccjoystick-field-factory-return-audit.txt"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
DETAILS="$WORK_ROOT/ccjoystick-field-factory-provenance-audit.jsonl"
AUDIT="$WORK_ROOT/ccjoystick-field-factory-provenance-audit.txt"

for required in \
  "$REPO_ROOT/tools/ghidra/ExportExactParameterFieldFactoryStore.java" \
  "$REPO_ROOT/tools/ghidra/ExportFunctionReturnObjectProvenance.java" \
  "$REPO_ROOT/tools/ml/plan_nevergone_ccjoystick_field_factory_return.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_call_root_factory_returns.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_ccjoystick_field_factory_provenance.py"; do
  [[ -f "$required" ]] || { echo "missing required file: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$CATALOG" ]] || { echo "missing static vtable slot catalog: $CATALOG" >&2; exit 2; }
mkdir -p "$WORK_ROOT"

printf '== refresh exact parameter-relative owner evidence ==\n'
bash "$REPO_ROOT/tools/ml/run_nevergone_parameter_relative_receiver_field_probe.sh"

python3 - "$OWNER_AUDIT" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1],encoding='utf-8') if x.strip()]
assert len(rows)==1, rows
r=rows[0]
assert r.get('format')=='NEVERGONE.ParameterRelativeReceiverFieldAudit/2', r
assert r.get('status')=='parameter_relative_field_with_function_owner_evidence', r
assert r.get('ghidra_owner_class')=='CCJoyStick', r
assert r.get('field_offset')==0x104, r
print('parameter-relative owner gate: pass')
PY

printf '\n== export CCJoyStick::init parameter-field stores ==\n'
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportExactParameterFieldFactoryStore.java 002a6584 "$FIELD_STORES"

printf '\n== plan field factory return proof ==\n'
python3 "$REPO_ROOT/tools/ml/plan_nevergone_ccjoystick_field_factory_return.py" \
  "$OWNER_AUDIT" "$FIELD_STORES" "$FACTORY_PLAN"

printf '\n== export factory return constructor provenance ==\n'
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportFunctionReturnObjectProvenance.java "$FACTORY_PLAN" "$FACTORY_PROV"

printf '\n== factory return audit ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_call_root_factory_returns.py" \
  "$FACTORY_PROV" "$CATALOG" --details-output "$FACTORY_AUDIT" | tee "$FACTORY_AUDIT_TXT"

printf '\n== CCJoyStick field factory provenance audit ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_ccjoystick_field_factory_provenance.py" \
  "$OWNER_AUDIT" "$FIELD_STORES" "$FACTORY_AUDIT" "$CATALOG" \
  --details-output "$DETAILS" | tee "$AUDIT"

printf '\nfield stores: %s\n' "$FIELD_STORES"
printf 'factory plan: %s\n' "$FACTORY_PLAN"
printf 'factory provenance: %s\n' "$FACTORY_PROV"
printf 'factory audit: %s\n' "$FACTORY_AUDIT"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT"
printf 'affects routing: no\n'
