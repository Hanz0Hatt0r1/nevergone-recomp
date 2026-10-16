#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${SKIP_GHIDRA_EXPORT:=0}"
DEFAULT_REVENGINSTR="$(cd "$REPO_ROOT/.." && pwd)/revenginstr"
REVENGINSTR_REPO=${REVENGINSTR_REPO:-$DEFAULT_REVENGINSTR}

EVID="$WORK_ROOT/evidence/beaten-callback-selectors-v30"
PCODE="$WORK_ROOT/evidence/residual-closure-v24/pcode/pcode-002a3e94.tsv"
OUT="$WORK_ROOT/beaten-callback-selectors-audit-v30.json"
ARCH="$HOME/nevergone-beaten-callback-selectors-v30.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

[[ -f "$PCODE" ]] || { echo "missing v24 p-code: $PCODE" >&2; exit 2; }
mkdir -p "$EVID"

if [[ "$SKIP_GHIDRA_EXPORT" != 1 ]]; then
  [[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
  if [[ -f "$REVENGINSTR_REPO/ghidra/ExportGhidraFunctionOperands.java" ]]; then
    GENERIC_OPERANDS="$EVID/generic-operands-002a3e94.jsonl"
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
      -scriptPath "$REVENGINSTR_REPO/ghidra;$REPO_ROOT/tools/ghidra" \
      -postScript ExportGhidraFunctionOperands.java 002a3e94 "$GENERIC_OPERANDS"
    python3 "$REPO_ROOT/tools/ml/adapt_revenginstr_function_operands.py" \
      "$GENERIC_OPERANDS" "$EVID/operands-002a3e94.tsv"
  else
    "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
      -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
      -scriptPath "$REPO_ROOT/tools/ghidra" \
      -postScript ExportFunctionOperands.java 002a3e94 "$EVID/operands-002a3e94.tsv"
  fi
  for spec in \
    'selector-addresses.tsv 0069ae6e 0069ad18 002a4178 002a4180' \
    'selector-cells.tsv 0093ede4 0093ede8' \
    'callback-targets.tsv 002a3a20 002a3c74'; do
    read -r -a parts <<< "$spec"
    output="${parts[0]}"
    if [[ -f "$REVENGINSTR_REPO/ghidra/ExportGhidraAddressMetadata.java" ]]; then
      GENERIC_OUTPUT="$EVID/${output%.tsv}.jsonl"
      "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
        -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
        -scriptPath "$REVENGINSTR_REPO/ghidra;$REPO_ROOT/tools/ghidra" \
        -postScript ExportGhidraAddressMetadata.java "$GENERIC_OUTPUT" "${parts[@]:1}"
      python3 "$REPO_ROOT/tools/ml/adapt_revenginstr_address_metadata.py" \
        "$GENERIC_OUTPUT" "$EVID/$output"
    else
      "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
        -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
        -scriptPath "$REPO_ROOT/tools/ghidra" \
        -postScript ExportAddressClassification.java "$EVID/$output" "${parts[@]:1}"
    fi
  done
fi

for input in "$EVID/operands-002a3e94.tsv" "$EVID/selector-addresses.tsv" \
  "$EVID/selector-cells.tsv" "$EVID/callback-targets.tsv"; do
  [[ -f "$input" ]] || { echo "missing evidence: $input" >&2; exit 2; }
done

python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_callback_selectors_v30.py" \
  "$EVID/operands-002a3e94.tsv" "$PCODE" \
  "$EVID/selector-addresses.tsv" "$EVID/selector-cells.tsv" \
  "$EVID/callback-targets.tsv" --output "$OUT"
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-callback-selectors-audit-v30.json \
  evidence/beaten-callback-selectors-v30 \
  evidence/residual-closure-v24/pcode/pcode-002a3e94.tsv
printf 'archive: %s\n' "$ARCH"
