#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

if [[ $# -ne 1 ]]; then
  echo "Usage: bash $0 /path/to/com.hippiegame.nevergone.apk" >&2
  exit 2
fi
APK="$1"
V37="$WORK_ROOT/beaten-script-engine-v37.json"
DLSYM="$WORK_ROOT/controls-dlsym-closure.jsonl"
OUT="$WORK_ROOT/beaten-script-engine-v38.json"
AUDIT="$WORK_ROOT/beaten-script-engine-v38.txt"
ARCH="$HOME/nevergone-beaten-script-engine-v38.tar.gz"

for required in "$APK" \
  "$REPO_ROOT/tools/ml/run_nevergone_beaten_script_engine_v37.sh" \
  "$REPO_ROOT/tools/ml/run_nevergone_controls_dlsym_closure.sh" \
  "$REPO_ROOT/tools/ml/audit_nevergone_beaten_script_engine_v38.py"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done

mkdir -p "$WORK_ROOT"

archive_partial() {
  local status=$?
  local items=()
  for rel in \
    beaten-script-engine-v38.json beaten-script-engine-v38.txt \
    beaten-script-engine-v37.json controls-dlsym-closure.jsonl \
    controls-dlsym-closure.txt controls-parent-script-static-closure.jsonl; do
    [[ -f "$WORK_ROOT/$rel" ]] && items+=("$rel")
  done
  if ((${#items[@]})); then
    tar -C "$WORK_ROOT" -czf "$ARCH" "${items[@]}" || true
    echo "partial archive: $ARCH" >&2
  fi
  exit "$status"
}
trap archive_partial ERR

bash "$REPO_ROOT/tools/ml/run_nevergone_beaten_script_engine_v37.sh" "$APK"
bash "$REPO_ROOT/tools/ml/run_nevergone_controls_dlsym_closure.sh" "$APK"

PYTHONPATH="$REPO_ROOT/tools/ml${PYTHONPATH:+:$PYTHONPATH}" \
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_script_engine_v38.py" \
  "$V37" "$DLSYM" --output "$OUT" | tee "$AUDIT"

tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-script-engine-v38.json beaten-script-engine-v38.txt \
  beaten-script-engine-v37.json controls-dlsym-closure.jsonl \
  controls-dlsym-closure.txt controls-parent-script-static-closure.jsonl
trap - ERR
printf 'archive: %s\n' "$ARCH"
