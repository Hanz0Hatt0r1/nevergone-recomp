#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage:
  tools/ghidra/run_player_current_frame_probe.sh \
    <ghidra-home> <project-dir> <project-name> <program-name> [output-dir]

Runs the bounded MeActionsSystem +0x1a4 STORE-candidate exporter against an
already analyzed Ghidra program, then validates/summarizes the derived metadata.
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
REVENGINSTR_REPO="${REVENGINSTR_REPO:-$REPO_ROOT/../revenginstr}"
SHARED_RUNNER="$REVENGINSTR_REPO/scripts/run_ghidra_field_store_candidates.sh"

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
if [[ ! -f "$SUMMARIZER" ]]; then
  echo 'Nevergone field-store summary tool is missing.' >&2
  exit 2
fi
if [[ ! -x "$SHARED_RUNNER" && ! -f "$EXPORTER" ]]; then
  echo 'Neither the shared nor local field-store exporter is available.' >&2
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

if [[ -x "$SHARED_RUNNER" ]]; then
  GHIDRA_HOME="$GHIDRA_HOME" "$SHARED_RUNNER" \
    "$PROJECT_DIR" "$PROJECT_NAME" "$PROGRAM_NAME" \
    'MeActionsSystem' 0x1a4 "$TSV"
else
  "$ANALYZE_HEADLESS" \
    "$PROJECT_DIR" \
    "$PROJECT_NAME" \
    -process "$PROGRAM_NAME" \
    -noanalysis \
    -scriptPath "$SCRIPT_DIR" \
    -postScript ExportFieldStoreCandidates.java \
      'MeActionsSystem' \
      0x1a4 \
      "$TSV"
fi

if [[ ! -s "$TSV" ]]; then
  echo "Probe did not produce field-store metadata: $TSV" >&2
  exit 1
fi

python3 "$SUMMARIZER" "$TSV" --field-offset 0x1a4 | tee "$SUMMARY"

echo "player current-frame probe complete: $OUTPUT_DIR"
