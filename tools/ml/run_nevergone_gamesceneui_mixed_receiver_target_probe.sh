#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
CALLER_SUBTREES="$WORK_ROOT/caller-argument-subtrees.tsv"
ACCESSOR_AUDIT="$WORK_ROOT/gamesceneui-controls-accessor-audit.jsonl"
CONSUMER_AUDIT="$WORK_ROOT/gamesceneui-controls-return-consumer-audit.jsonl"
SINGLETON_PROVENANCE="$WORK_ROOT/gamesceneui-singleton-store-return.tsv"
SINGLETON_AUDIT="$WORK_ROOT/gamesceneui-singleton-store-return-audit.jsonl"
SINGLETON_AUDIT_TXT="$WORK_ROOT/gamesceneui-singleton-store-return-audit.txt"
DETAILS="$WORK_ROOT/gamesceneui-mixed-target-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/gamesceneui-mixed-target-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in "$CATALOG" "$CALLER_SUBTREES" "$ACCESSOR_AUDIT"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportSingletonStoreReturnProvenance.java" ]] || { echo "missing singleton exporter" >&2; exit 2; }
[[ -x "$REPO_ROOT/tools/ml/run_nevergone_gamesceneui_controls_return_consumer_probe.sh" ]] || { echo "missing controls consumer probe runner" >&2; exit 2; }

# Refresh the already-merged evidence that GetControlsObj() returns the +0x134
# field and that the exact returned value has one catalog member-consumer class.
REPO_ROOT="$REPO_ROOT" WORK_ROOT="$WORK_ROOT" GHIDRA_HOME="$GHIDRA_HOME" \
GHIDRA_PROJECT_DIR="$GHIDRA_PROJECT_DIR" GHIDRA_PROJECT_NAME="$GHIDRA_PROJECT_NAME" GHIDRA_PROGRAM="$GHIDRA_PROGRAM" \
  bash "$REPO_ROOT/tools/ml/run_nevergone_gamesceneui_controls_return_consumer_probe.sh"
[[ -f "$CONSUMER_AUDIT" ]] || { echo "controls consumer audit was not produced: $CONSUMER_AUDIT" >&2; exit 2; }

# Ghidra uses the established +0x10000 image-base view for this ARMv7 binary.
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportSingletonStoreReturnProvenance.java 003e95e0 "$SINGLETON_PROVENANCE"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_singleton_store_return.py" \
  "$SINGLETON_PROVENANCE" "$CATALOG" \
  --expected-function sharedGameSceneUI \
  --expected-class GameSceneUI \
  --details-output "$SINGLETON_AUDIT" \
  | tee "$SINGLETON_AUDIT_TXT"

echo
printf '== GameSceneUI mixed receiver target convergence ==\n'
python3 "$REPO_ROOT/tools/ml/audit_nevergone_gamesceneui_mixed_receiver_target.py" \
  "$SINGLETON_AUDIT" \
  "$CONSUMER_AUDIT" \
  "$CALLER_SUBTREES" \
  "$CATALOG" \
  --details-output "$DETAILS" \
  | tee "$AUDIT_TXT"

printf '\nsingleton provenance: %s\n' "$SINGLETON_PROVENANCE"
printf 'singleton audit: %s\n' "$SINGLETON_AUDIT"
printf 'controls consumer audit: %s\n' "$CONSUMER_AUDIT"
printf 'details: %s\n' "$DETAILS"
printf 'audit text: %s\n' "$AUDIT_TXT"
printf 'affects routing: no\n'
