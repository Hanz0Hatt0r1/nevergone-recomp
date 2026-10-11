#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage:
  tools/ghidra/run_player_current_frame_probe.sh \
    <ghidra-home> <project-dir> <project-name> <program-name> [output-dir]

Runs the bounded +0x294 STORE-candidate exporter against an already analyzed
Ghidra program, then validates/summarizes the derived TSV metadata.
EOF
  exit 2
}

[[ $# -ge 4 && $# -le 5 ]] || usage

GHIDRA_HOME="$1"
PROJECT_DIR="$2"
PROJECT_NAME="$3"
PROGRAM_NAME="$4"
OUTPUT_DIR="${5:-artifacts/player-current-frame-probe}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT_DIR="$REPO_ROOT/tools/ghidra"
ANALYZE_HEADLESS="$GHIDRA_HOME/support/analyzeHeadless"
EXPORTER="$SCRIPT_DIR/ExportFieldStoreCandidates.java"
SUMMARIZER="$SCRIPT_DIR/summarize_field_store_candidates.py"

if [[ ! -x "$ANALYZE_HEADLESS" ]]; then
  echo "Ghidra analyzeHeadless is not executable: $ANALYZE_HEADLESS" >&2
  exit 2
fi
if [[ ! -d "$PROJECT_DIR" ]]; then
  echo "Ghidra project directory does not exist: $PROJECT_DIR" >&2
  exit 2
fi
if [[ -z "$PROJECT_NAME" || -z "$PROGRAM_NAME" ]]; then
  echo 'Project name and program name must be non-empty.' >&2
  exit 2
fi
if [[ ! -f "$EXPORTER" || ! -f "$SUMMARIZER" ]]; then
  echo 'Required Nevergone Ghidra probe files are missing.' >&2
  exit 2
fi
command -v python3 >/dev/null 2>&1 || {
  echo 'python3 is required.' >&2
  exit 2
}

mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
TSV="$OUTPUT_DIR/player-current-frame-stores.tsv"
SUMMARY="$OUTPUT_DIR/player-current-frame-summary.txt"

rm -f "$TSV" "$SUMMARY"

"$ANALYZE_HEADLESS" \
  "$PROJECT_DIR" \
  "$PROJECT_NAME" \
  -process "$PROGRAM_NAME" \
  -noanalysis \
  -scriptPath "$SCRIPT_DIR" \
  -postScript ExportFieldStoreCandidates.java \
    'EnemyActionsSystem;MeActionsSystem' \
    0x294 \
    "$TSV"

if [[ ! -s "$TSV" ]]; then
  echo "Probe did not produce field-store metadata: $TSV" >&2
  exit 1
fi

python3 "$SUMMARIZER" "$TSV" --field-offset 0x294 | tee "$SUMMARY"

echo "player current-frame probe complete: $OUTPUT_DIR"
