#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${ARCHIVE:=$HOME/nevergone-controls-alias-operand-roots.tar.gz}"

if [[ $# -ne 2 ]]; then
  echo "Usage: bash $0 /path/to/nevergone-controls-field-alias-evidence.tar.gz /path/to/com.hippiegame.nevergone.apk" >&2
  exit 2
fi

EVIDENCE="$1"
ORIGINAL="$2"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
TRIAGE="$WORK_ROOT/controls-field-alias-triage.jsonl"
PLAN="$WORK_ROOT/controls-field-next-probe-plan.tsv"
GHIDRA="$WORK_ROOT/evidence/controls-alias-operand-roots.tsv"
DETAILS="$WORK_ROOT/controls-alias-operand-roots-audit.jsonl"
AUDIT="$WORK_ROOT/controls-alias-operand-roots-audit.txt"

for required in "$EVIDENCE" "$ORIGINAL" \
  "$REPO_ROOT/tools/ml/triage_nevergone_controls_field_alias_evidence.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_alias_operand_roots.py" \
  "$REPO_ROOT/tools/ghidra/ExportControlsAliasOperandRoots.java"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$(dirname "$GHIDRA")"

python3 "$REPO_ROOT/tools/ml/triage_nevergone_controls_field_alias_evidence.py" \
  "$EVIDENCE" "$ORIGINAL" --details-output "$TRIAGE" --plan-output "$PLAN"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportControlsAliasOperandRoots.java "$PLAN" "$GHIDRA"

# Preserve raw evidence when a site fails, even if strict audit exits nonzero.
set +e
python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_alias_operand_roots.py" \
  "$PLAN" "$GHIDRA" --details-output "$DETAILS" --require-complete | tee "$AUDIT"
status=${PIPESTATUS[0]}
set -e

tar -C "$WORK_ROOT" -czf "$ARCHIVE" \
  controls-field-alias-triage.jsonl \
  controls-field-next-probe-plan.tsv \
  evidence/controls-alias-operand-roots.tsv \
  controls-alias-operand-roots-audit.jsonl \
  controls-alias-operand-roots-audit.txt

printf '\nArchive: %s\nAudit exit: %s\nProprietary binary included: no\nAffects routing: no\n' \
  "$ARCHIVE" "$status"
exit "$status"
