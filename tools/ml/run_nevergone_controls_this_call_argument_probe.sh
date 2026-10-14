#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${ARCHIVE:=$HOME/nevergone-controls-this-call-arguments.tar.gz}"

if [[ $# -ne 2 ]]; then
  echo "Usage: bash $0 /path/to/nevergone-controls-field-alias-evidence.tar.gz /path/to/com.hippiegame.nevergone.apk" >&2
  exit 2
fi

EVIDENCE="$1"
ORIGINAL="$2"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
TRIAGE="$WORK_ROOT/controls-field-alias-triage.jsonl"
OLD_PLAN="$WORK_ROOT/controls-field-next-probe-plan.tsv"
PLAN="$WORK_ROOT/controls-this-call-argument-plan.tsv"
GHIDRA_OUT="$WORK_ROOT/evidence/controls-this-call-argument-offsets.tsv"
DETAILS="$WORK_ROOT/controls-this-call-argument-offset-audit.jsonl"
AUDIT="$WORK_ROOT/controls-this-call-argument-offset-audit.txt"

for required in "$EVIDENCE" "$ORIGINAL" \
  "$REPO_ROOT/tools/ml/triage_nevergone_controls_field_alias_evidence.py" \
  "$REPO_ROOT/tools/ml/plan_nevergone_controls_this_call_arguments.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_this_call_argument_offsets.py" \
  "$REPO_ROOT/tools/ghidra/ExportControlsThisCallArgumentOffsets.java"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$(dirname "$GHIDRA_OUT")"

# Revalidate the original 38-method evidence against the exact original ELF
# before deriving the larger 78-call plan.
python3 "$REPO_ROOT/tools/ml/triage_nevergone_controls_field_alias_evidence.py" \
  "$EVIDENCE" "$ORIGINAL" \
  --details-output "$TRIAGE" \
  --plan-output "$OLD_PLAN"

python3 "$REPO_ROOT/tools/ml/plan_nevergone_controls_this_call_arguments.py" \
  "$TRIAGE" --output "$PLAN"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportControlsThisCallArgumentOffsets.java "$PLAN" "$GHIDRA_OUT"

# Keep raw evidence available even when the strict audit rejects one or more
# sites. This lets the next revision diagnose Ghidra representation changes.
set +e
python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_this_call_argument_offsets.py" \
  "$PLAN" "$GHIDRA_OUT" \
  --details-output "$DETAILS" --require-complete | tee "$AUDIT"
status=${PIPESTATUS[0]}
set -e

tar -C "$WORK_ROOT" -czf "$ARCHIVE" \
  controls-field-alias-triage.jsonl \
  controls-this-call-argument-plan.tsv \
  evidence/controls-this-call-argument-offsets.tsv \
  controls-this-call-argument-offset-audit.jsonl \
  controls-this-call-argument-offset-audit.txt

printf '\nArchive: %s\nAudit exit: %s\nOriginal binary included: no\nAffects routing: no\n' \
  "$ARCHIVE" "$status"
exit "$status"
