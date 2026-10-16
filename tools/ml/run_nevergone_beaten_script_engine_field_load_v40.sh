#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
source "$REPO_ROOT/tools/ghidra/revenginstr_pcode_flow_script_path.sh"
PCODE_SCRIPT_PATH=$(revenginstr_pcode_flow_script_path "$REPO_ROOT")
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

V39="$WORK_ROOT/beaten-script-engine-manager-v39.json"
EXPORT="$WORK_ROOT/beaten-script-engine-field-load-v40.tsv"
SHARED_PCODE="$WORK_ROOT/beaten-script-engine-v40-shared-manager-pcode.tsv"
PURGE_PCODE="$WORK_ROOT/beaten-script-engine-v40-purge-manager-pcode.tsv"
OUT="$WORK_ROOT/beaten-script-engine-field-load-v40.json"
AUDIT="$WORK_ROOT/beaten-script-engine-field-load-v40.txt"
ARCH="$HOME/nevergone-beaten-script-engine-field-load-v40.tar.gz"

if (($# > 1)); then
  echo "Usage: bash $0 [/path/to/com.hippiegame.nevergone.apk]" >&2
  exit 2
fi
APK="${1:-}"

for required in \
  "$REPO_ROOT/tools/ghidra/ExportAccessorFieldLoadConsumers.java" \
  "$REPO_ROOT/tools/ghidra/ExportFunctionPcodeFlow.java" \
  "$REPO_ROOT/tools/ml/audit_nevergone_beaten_script_engine_field_load_v40.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$WORK_ROOT"

if [[ ! -f "$V39" ]]; then
  [[ -n "$APK" && -f "$APK" ]] || {
    echo "missing v39 report: $V39; pass the original APK so v39 can be regenerated" >&2
    exit 2
  }
  bash "$REPO_ROOT/tools/ml/run_nevergone_beaten_script_engine_manager_v39.sh" "$APK"
fi

archive_partial() {
  local status=$?
  local items=()
  for rel in \
    beaten-script-engine-manager-v39.json \
    beaten-script-engine-field-load-v40.tsv \
    beaten-script-engine-v40-shared-manager-pcode.tsv \
    beaten-script-engine-v40-purge-manager-pcode.tsv \
    beaten-script-engine-field-load-v40.json \
    beaten-script-engine-field-load-v40.txt; do
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

run_pcode_flow() {
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$PCODE_SCRIPT_PATH" "$@"
}

echo "== sharedManager engine-field load consumers =="
run_ghidra -postScript ExportAccessorFieldLoadConsumers.java \
  "cocos2d::CCScriptEngineManager::sharedManager" "$EXPORT"

echo "== sharedManager p-code for backing-cell follow-up =="
run_pcode_flow -postScript ExportFunctionPcodeFlow.java \
  "cocos2d::CCScriptEngineManager::sharedManager" "$SHARED_PCODE"

echo "== purgeSharedManager p-code for backing-cell follow-up =="
run_pcode_flow -postScript ExportFunctionPcodeFlow.java \
  "cocos2d::CCScriptEngineManager::purgeSharedManager" "$PURGE_PCODE"

echo "== v40 engine-field load audit =="
PYTHONPATH="$REPO_ROOT/tools/ml${PYTHONPATH:+:$PYTHONPATH}" \
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_script_engine_field_load_v40.py" \
  "$V39" "$EXPORT" --output "$OUT" | tee "$AUDIT"

tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-script-engine-manager-v39.json \
  beaten-script-engine-field-load-v40.tsv \
  beaten-script-engine-v40-shared-manager-pcode.tsv \
  beaten-script-engine-v40-purge-manager-pcode.tsv \
  beaten-script-engine-field-load-v40.json \
  beaten-script-engine-field-load-v40.txt
trap - ERR
printf 'archive: %s\nengine-field load evidence only; receiver promotions: 0\naffects routing: no\n' "$ARCH"
