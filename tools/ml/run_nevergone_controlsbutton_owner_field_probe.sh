#!/usr/bin/env bash
set -euo pipefail
: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${ARCHIVE:=$HOME/nevergone-controlsbutton-owner-field-evidence.tar.gz}"
if [[ $# -ne 1 ]]; then echo "Usage: bash $0 /path/to/com.hippiegame.nevergone.apk" >&2; exit 2; fi
APK="$1"; ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
PLAN="$WORK_ROOT/controlsbutton-owner-field-native-plan.jsonl"; METHODS="$WORK_ROOT/controlsbutton-owner-methods.tsv"; GH="$WORK_ROOT/evidence/controlsbutton-owner-field-aliases.tsv"; DETAILS="$WORK_ROOT/controlsbutton-owner-field-alias-audit.jsonl"; AUDIT="$WORK_ROOT/controlsbutton-owner-field-alias-audit.txt"
for f in "$APK" "$REPO_ROOT/tools/ml/plan_nevergone_controlsbutton_owner_field_probe.py" "$REPO_ROOT/tools/ghidra/ExportControlsButtonOwnerFieldAliasInventory.java" "$REPO_ROOT/tools/ml/audit_nevergone_controlsbutton_owner_field_probe.py"; do [[ -f "$f" ]] || { echo "missing: $f" >&2; exit 2; }; done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$(dirname "$GH")"
python3 "$REPO_ROOT/tools/ml/plan_nevergone_controlsbutton_owner_field_probe.py" "$APK" --methods-output "$METHODS" --details-output "$PLAN"
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis -scriptPath "$REPO_ROOT/tools/ghidra" -postScript ExportControlsButtonOwnerFieldAliasInventory.java "$METHODS" "$GH"
set +e
python3 "$REPO_ROOT/tools/ml/audit_nevergone_controlsbutton_owner_field_probe.py" "$PLAN" "$METHODS" "$GH" --details-output "$DETAILS" --require-complete | tee "$AUDIT"
status=${PIPESTATUS[0]}
set -e
tar -C "$WORK_ROOT" -czf "$ARCHIVE" controlsbutton-owner-field-native-plan.jsonl controlsbutton-owner-methods.tsv evidence/controlsbutton-owner-field-aliases.tsv controlsbutton-owner-field-alias-audit.jsonl controlsbutton-owner-field-alias-audit.txt
printf '\nArchive: %s\nAudit exit: %s\nProprietary binary included: no\nAffects routing: no\n' "$ARCHIVE" "$status"
exit "$status"
