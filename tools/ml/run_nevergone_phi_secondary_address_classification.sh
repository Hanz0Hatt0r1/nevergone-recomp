#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${OUT:=$WORK_ROOT/phi-secondary-address-classification.tsv}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportAddressClassification.java" ]] || {
  echo "missing address classifier: $REPO_ROOT/tools/ghidra/ExportAddressClassification.java" >&2
  exit 2
}
mkdir -p "$(dirname "$OUT")"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java \
    "$OUT" \
    002950ac 002950be 00295134 00295138 \
    0029517c 0029518a 002951fc 00295200 \
    002a647e 00549614

printf '\n== PHI secondary address classification ==\n'
cat "$OUT"
printf '\noutput: %s\n' "$OUT"
printf 'affects routing: no\n'
