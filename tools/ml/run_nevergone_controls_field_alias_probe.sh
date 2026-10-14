#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${ARCHIVE:=$HOME/nevergone-controls-field-alias-evidence.tar.gz}"

if [[ $# -ne 1 ]]; then
  echo "Usage: bash $0 /path/to/com.hippiegame.nevergone.apk" >&2
  exit 2
fi
ORIGINAL="$1"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
NATIVE="$WORK_ROOT/controls-field-native-store-inventory.jsonl"
METHODS="$WORK_ROOT/controls-field-native-methods.tsv"
GHIDRA="$WORK_ROOT/evidence/controls-field-write-aliases.tsv"
DETAILS="$WORK_ROOT/controls-field-write-alias-audit.jsonl"
AUDIT="$WORK_ROOT/controls-field-write-alias-audit.txt"

for required in "$ORIGINAL" "$ANALYZE" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_field_native_store_inventory.py" \
  "$REPO_ROOT/tools/ghidra/ExportControlsFieldWriteAliasInventory.java" \
  "$REPO_ROOT/tools/ml/audit_nevergone_controls_field_write_aliases.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "analyzeHeadless is not executable: $ANALYZE" >&2; exit 2; }
mkdir -p "$(dirname "$GHIDRA")"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_field_native_store_inventory.py" \
  "$ORIGINAL" --details-output "$NATIVE" --methods-output "$METHODS"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportControlsFieldWriteAliasInventory.java "$METHODS" "$GHIDRA"

# Preserve raw evidence even when a strict audit finds an unexpected writer,
# missing method, or decompiler issue.
set +e
python3 "$REPO_ROOT/tools/ml/audit_nevergone_controls_field_write_aliases.py" \
  "$NATIVE" "$METHODS" "$GHIDRA" \
  --details-output "$DETAILS" --require-complete | tee "$AUDIT"
status=${PIPESTATUS[0]}
set -e

tar -C "$WORK_ROOT" -czf "$ARCHIVE" \
  controls-field-native-store-inventory.jsonl \
  controls-field-native-methods.tsv \
  evidence/controls-field-write-aliases.tsv \
  controls-field-write-alias-audit.jsonl \
  controls-field-write-alias-audit.txt

printf '\nArchive: %s\nAudit exit: %s\nProprietary binary included: no\nAffects routing: no\n' \
  "$ARCHIVE" "$status"
exit "$status"
