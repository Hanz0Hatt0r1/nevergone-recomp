#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${OUT_ARCHIVE:=$HOME/nevergone-controls-full-this-callee-frontier.tar.gz}"

if [[ $# -ne 1 ]]; then
  echo "Usage: bash $0 /path/to/nevergone-controls-this-call-arguments.tar.gz" >&2
  exit 2
fi

ARCHIVE="$1"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
TRIAGE="$WORK_ROOT/controls-field-alias-triage.jsonl"
OFFSET_AUDIT="$WORK_ROOT/controls-this-call-argument-offset-audit.jsonl"
SOURCE_PLAN="$WORK_ROOT/controls-this-call-argument-plan.tsv"
PLAN="$WORK_ROOT/controls-full-this-callee-frontier-plan.tsv"
GHIDRA_OUT="$WORK_ROOT/evidence/controls-full-this-callee-parameter-uses.tsv"
DETAILS="$WORK_ROOT/controls-full-this-callee-frontier-audit.jsonl"
AUDIT="$WORK_ROOT/controls-full-this-callee-frontier-audit.txt"

for required in "$ARCHIVE" \
  "$REPO_ROOT/tools/ml/plan_nevergone_controls_full_this_callee_frontier.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_full_this_callee_frontier.py" \
  "$REPO_ROOT/tools/ghidra/ExportControlsFullThisCalleeParameterUses.java"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$WORK_ROOT/evidence"

tar -xOf "$ARCHIVE" controls-field-alias-triage.jsonl > "$TRIAGE"
tar -xOf "$ARCHIVE" controls-this-call-argument-offset-audit.jsonl > "$OFFSET_AUDIT"
tar -xOf "$ARCHIVE" controls-this-call-argument-plan.tsv > "$SOURCE_PLAN"

python3 "$REPO_ROOT/tools/ml/plan_nevergone_controls_full_this_callee_frontier.py" \
  "$TRIAGE" "$OFFSET_AUDIT" "$SOURCE_PLAN" --output "$PLAN"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportControlsFullThisCalleeParameterUses.java "$PLAN" "$GHIDRA_OUT"

set +e
python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_full_this_callee_frontier.py" \
  "$PLAN" "$GHIDRA_OUT" --details-output "$DETAILS" --require-complete | tee "$AUDIT"
status=${PIPESTATUS[0]}
set -e

tar -C "$WORK_ROOT" -czf "$OUT_ARCHIVE" \
  controls-full-this-callee-frontier-plan.tsv \
  evidence/controls-full-this-callee-parameter-uses.tsv \
  controls-full-this-callee-frontier-audit.jsonl \
  controls-full-this-callee-frontier-audit.txt

printf '\nArchive: %s\nAudit exit: %s\nOriginal binary included: no\nAffects routing: no\n' \
  "$OUT_ARCHIVE" "$status"
exit "$status"
