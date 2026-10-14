#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${DETAILS:=$WORK_ROOT/phi-secondary-load-tree-audit.jsonl}"
: "${OUT:=$WORK_ROOT/phi-secondary-resolved-address-classification.tsv}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$DETAILS" ]] || { echo "missing corrected PHI audit details: $DETAILS" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportAddressClassification.java" ]] || {
  echo "missing address classifier" >&2
  exit 2
}

mapfile -t TARGETS < <(python3 - "$DETAILS" <<'PY'
import json, sys
addresses = set()
with open(sys.argv[1], encoding="utf-8") as fh:
    for line in fh:
        if not line.strip():
            continue
        row = json.loads(line)
        if row.get("format") != "NEVERGONE.PhiSecondaryLoadTreeAudit/2":
            raise SystemExit(f"unexpected audit format: {row.get('format')}")
        addresses.update(int(x) for x in row.get("unresolved_load_addresses", []))
for address in sorted(addresses):
    print(f"{address:08x}")
PY
)

((${#TARGETS[@]} > 0)) || { echo "no unresolved LOAD targets to classify" >&2; exit 3; }
mkdir -p "$(dirname "$OUT")"

printf 'derived unresolved targets:'
printf ' %s' "${TARGETS[@]}"
printf '\n'

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressClassification.java "$OUT" "${TARGETS[@]}"

printf '\n== corrected PHI unresolved address classification ==\n'
cat "$OUT"
printf '\noutput: %s\n' "$OUT"
printf 'affects routing: no\n'
