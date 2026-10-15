#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
BEAT="$WORK_ROOT/residual-owner-field-writes-v21-beateneffect.tsv"
JOY="$WORK_ROOT/residual-owner-field-writes-v21-ccjoystick.tsv"
DETAILS="$WORK_ROOT/residual-owner-field-writes-v21-audit.jsonl"
AUDIT="$WORK_ROOT/residual-owner-field-writes-v21-audit.txt"
ARCH="$HOME/nevergone-residual-owner-field-writes-v21.tar.gz"

for f in \
  "$REPO_ROOT/tools/ghidra/ExportClassParameterFieldWrites.java" \
  "$REPO_ROOT/tools/ml/audit_nevergone_residual_owner_field_writes_v21.py"; do
  [[ -f "$f" ]] || { echo "missing: $f" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$WORK_ROOT"

for spec in "BeatenEffect:$BEAT" "CCJoyStick:$JOY"; do
  cls="${spec%%:*}"; out="${spec#*:}"
  echo "== exact parameter-field writes for $cls =="
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportClassParameterFieldWrites.java "$cls" "$out"
done

echo
echo "== residual owner field-write audit =="
set +e
python3 "$REPO_ROOT/tools/ml/audit_nevergone_residual_owner_field_writes_v21.py" \
  "$BEAT" "$JOY" --details-output "$DETAILS" --require-observed-targets \
  | tee "$AUDIT"
status=${PIPESTATUS[0]}
set -e

for f in "$BEAT" "$JOY" "$DETAILS" "$AUDIT"; do
  [[ -f "$f" ]] || { echo "missing diagnostic member: $f" >&2; exit 2; }
done
tar -C "$WORK_ROOT" -czf "$ARCH" \
  "$(basename "$BEAT")" "$(basename "$JOY")" "$(basename "$DETAILS")" "$(basename "$AUDIT")"
printf '\narchive: %s\nreceiver class proven: no\naffects routing: no\n' "$ARCH"
if (( status != 0 )); then
  echo "strict target-write gate failed; diagnostic archive preserved" >&2
  exit "$status"
fi
