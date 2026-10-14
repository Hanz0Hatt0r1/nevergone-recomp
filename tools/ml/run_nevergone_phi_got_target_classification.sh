#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${GOT_CLASSIFICATION:=$WORK_ROOT/phi-secondary-resolved-address-classification.tsv}"
: "${OUT:=$WORK_ROOT/phi-secondary-got-target-classification.tsv}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$GOT_CLASSIFICATION" ]] || { echo "missing corrected GOT classification: $GOT_CLASSIFICATION" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportAddressClassification.java" ]] || {
  echo "missing address classifier: $REPO_ROOT/tools/ghidra/ExportAddressClassification.java" >&2
  exit 2
}
mkdir -p "$(dirname "$OUT")"

mapfile -t TARGETS < <(python3 - "$GOT_CLASSIFICATION" <<'PY'
import csv
import sys
from pathlib import Path

path = Path(sys.argv[1])
rows = list(csv.DictReader(path.open("r", encoding="utf-8", newline=""), delimiter="\t"))
targets = []
for row in rows:
    if row.get("format") != "NEVERGONE.GhidraAddressClassification/1":
        raise SystemExit(f"unexpected format in {path}: {row.get('format')}")
    if row.get("block") != ".got":
        continue
    if not (row.get("primary_symbol") or "").startswith("PTR_default_instance__"):
        continue
    raw = (row.get("raw_u32") or "").strip()
    if not raw or raw == "-":
        continue
    value = int(raw, 0) & 0xffffffff
    targets.append(f"{value:08x}")

targets = sorted(set(targets))
if len(targets) != 2:
    raise SystemExit(f"expected exactly 2 default-instance GOT targets, got {targets}")
for target in targets:
    print(target)
PY
)

printf 'derived GOT targets:'
printf ' %s' "${TARGETS[@]}"
printf '\n'

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java \
    "$OUT" "${TARGETS[@]}"

printf '\n== PHI GOT target classification ==\n'
cat "$OUT"
printf '\noutput: %s\n' "$OUT"
printf 'affects routing: no\n'
