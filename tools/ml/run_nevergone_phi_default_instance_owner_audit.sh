#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${TARGET_CLASSIFICATION:=$WORK_ROOT/phi-secondary-got-target-classification.tsv}"
: "${OUT:=$WORK_ROOT/phi-default-instance-owner-references.tsv}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$TARGET_CLASSIFICATION" ]] || { echo "missing GOT target classification: $TARGET_CLASSIFICATION" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportAddressReferenceOwners.java" ]] || {
  echo "missing address reference-owner exporter" >&2
  exit 2
}

mapfile -t TARGETS < <(python3 - "$TARGET_CLASSIFICATION" <<'PY'
import csv, sys
path = sys.argv[1]
rows = []
with open(path, encoding="utf-8", newline="") as f:
    for row in csv.DictReader(f, delimiter="\t"):
        if row.get("format") != "NEVERGONE.GhidraAddressClassification/1":
            continue
        if row.get("block") != ".bss":
            continue
        if row.get("primary_symbol") != "default_instance_":
            continue
        address = (row.get("address") or "").strip()
        if address:
            rows.append(address)
rows = sorted(set(rows))
if len(rows) != 2:
    raise SystemExit(f"expected exactly 2 .bss default_instance_ targets, got {len(rows)}: {rows}")
print("\n".join(rows))
PY
)

printf 'derived default-instance targets: %s\n' "${TARGETS[*]}"
mkdir -p "$(dirname "$OUT")"

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" \
  -recursive \
  -readOnly \
  -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportAddressReferenceOwners.java "$OUT" "${TARGETS[@]}"

printf '\n== PHI default-instance owner references ==\n'
cat "$OUT"
printf '\noutput: %s\n' "$OUT"
printf 'affects routing: no\n'
