#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

V38="$WORK_ROOT/beaten-script-engine-v38.json"
FIELD="$WORK_ROOT/beaten-script-engine-manager-v39-field-writes.tsv"
FLOW="$WORK_ROOT/beaten-script-engine-manager-v39-owner-flow.tsv"
CONSUMERS="$WORK_ROOT/beaten-script-engine-manager-v39-shared-consumers.tsv"
OUT="$WORK_ROOT/beaten-script-engine-manager-v39.json"
AUDIT="$WORK_ROOT/beaten-script-engine-manager-v39.txt"
ARCH="$HOME/nevergone-beaten-script-engine-manager-v39.tar.gz"

if (($# > 1)); then
  echo "Usage: bash $0 [/path/to/com.hippiegame.nevergone.apk]" >&2
  exit 2
fi
APK="${1:-}"

for required in \
  "$REPO_ROOT/tools/ghidra/ExportClassParameterFieldWrites.java" \
  "$REPO_ROOT/tools/ghidra/ExportClassOwnerAliasFlow.java" \
  "$REPO_ROOT/tools/ghidra/ExportAccessorReturnConsumers.java" \
  "$REPO_ROOT/tools/ml/audit_nevergone_beaten_script_engine_manager_v39.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$WORK_ROOT"

if [[ ! -f "$V38" ]]; then
  [[ -n "$APK" && -f "$APK" ]] || {
    echo "missing v38 report: $V38; pass the original APK so v38 can be regenerated" >&2
    exit 2
  }
  bash "$REPO_ROOT/tools/ml/run_nevergone_beaten_script_engine_v38.sh" "$APK"
fi

archive_partial() {
  local status=$?
  local items=()
  for rel in \
    beaten-script-engine-v38.json \
    beaten-script-engine-manager-v39-field-writes.tsv \
    beaten-script-engine-manager-v39-owner-flow.tsv \
    beaten-script-engine-manager-v39-shared-consumers.tsv \
    beaten-script-engine-manager-v39.json \
    beaten-script-engine-manager-v39.txt; do
    [[ -f "$WORK_ROOT/$rel" ]] && items+=("$rel")
  done
  if ((${#items[@]})); then
    tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}" || true
    echo "partial archive: $ARCH" >&2
  fi
  exit "$status"
}
trap archive_partial ERR

run_ghidra() {
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" "$@"
}

echo "== CCScriptEngineManager parameter-relative field writes =="
run_ghidra -postScript ExportClassParameterFieldWrites.java \
  "cocos2d::CCScriptEngineManager" "$FIELD"

echo "== CCScriptEngineManager precise owner flow =="
run_ghidra -postScript ExportClassOwnerAliasFlow.java \
  "cocos2d::CCScriptEngineManager" "$FLOW"

echo "== sharedManager direct return consumers =="
# Use the exact analyzed symbol rather than an ELF virtual address. The local
# Ghidra project can carry an image-base delta relative to the packaged ELF.
run_ghidra -postScript ExportAccessorReturnConsumers.java \
  "cocos2d::CCScriptEngineManager::sharedManager" "$CONSUMERS"

echo "== v39 manager/consumer frontier audit =="
PYTHONPATH="$REPO_ROOT/tools/ml${PYTHONPATH:+:$PYTHONPATH}" \
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_script_engine_manager_v39.py" \
  "$V38" "$FIELD" "$FLOW" "$CONSUMERS" --output "$OUT" | tee "$AUDIT"

tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-script-engine-v38.json \
  beaten-script-engine-manager-v39-field-writes.tsv \
  beaten-script-engine-manager-v39-owner-flow.tsv \
  beaten-script-engine-manager-v39-shared-consumers.tsv \
  beaten-script-engine-manager-v39.json \
  beaten-script-engine-manager-v39.txt
trap - ERR
printf 'archive: %s\nmanager/consumer evidence only; receiver promotions: 0\naffects routing: no\n' "$ARCH"
