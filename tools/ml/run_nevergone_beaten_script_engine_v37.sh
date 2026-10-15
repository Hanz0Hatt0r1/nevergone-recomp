#!/usr/bin/env bash
set -euo pipefail

: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"

if [[ $# -ne 1 ]]; then
  echo "Usage: bash $0 /path/to/com.hippiegame.nevergone.apk" >&2
  exit 2
fi
APK="$1"
EVID="$WORK_ROOT/evidence/beaten-child-lifecycle-v36"
V36="$WORK_ROOT/beaten-child-lifecycle-v36.json"
CONTROLS="$WORK_ROOT/controls-parent-script-static-closure.jsonl"
OUT="$WORK_ROOT/beaten-script-engine-v37.json"
ARCH="$HOME/nevergone-beaten-script-engine-v37.tar.gz"

for input in "$APK" "$V36" "$REPO_ROOT/tools/ml/audit_nevergone_controls_parent_script_static_closure.py"; do
  [[ -f "$input" ]] || { echo "missing required input: $input" >&2; exit 2; }
done
for fn in 005270f8 00525fe0 00526fce; do
  [[ -f "$EVID/pcode-$fn.tsv" ]] || { echo "missing lifecycle p-code: $fn" >&2; exit 2; }
done

bash "$REPO_ROOT/tools/ml/run_nevergone_controls_parent_script_static_closure.sh" "$APK"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_beaten_script_engine_v37.py" \
  "$APK" "$EVID" "$V36" --output "$OUT"
tar -C "$WORK_ROOT" -czf "$ARCH" \
  beaten-script-engine-v37.json beaten-child-lifecycle-v36.json \
  controls-parent-script-static-closure.jsonl \
  evidence/beaten-child-lifecycle-v36/pcode-005270f8.tsv \
  evidence/beaten-child-lifecycle-v36/pcode-00525fe0.tsv \
  evidence/beaten-child-lifecycle-v36/pcode-00526fce.tsv
printf 'archive: %s\n' "$ARCH"
