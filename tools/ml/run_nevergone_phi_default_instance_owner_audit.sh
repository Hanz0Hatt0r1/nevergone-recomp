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

source "$REPO_ROOT/tools/ghidra/revenginstr_pcode_flow_script_path.sh"
ADDRESS_REFERENCE_SCRIPT_PATH=$(revenginstr_address_reference_script_path "$REPO_ROOT")
DEFAULT_REVENGINSTR="$(cd "$REPO_ROOT/.." && pwd)/revenginstr"
SHARED_GHIDRA_DIR="${REVENGINSTR_REPO:-$DEFAULT_REVENGINSTR}/ghidra"
ADDRESS_REFERENCE_ARGS=()
if [[ -f "$SHARED_GHIDRA_DIR/ExportGhidraAddressReferenceOwners.java" ]]; then
  ADDRESS_REFERENCE_ARGS=(--shared-ghidra-script-dir "$SHARED_GHIDRA_DIR")
fi

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
  -scriptPath "$ADDRESS_REFERENCE_SCRIPT_PATH" \
  -postScript ExportAddressReferenceOwners.java "$OUT" "${TARGETS[@]}" "${ADDRESS_REFERENCE_ARGS[@]}"

printf '\n== PHI default-instance owner references ==\n'
cat "$OUT"
printf '\noutput: %s\n' "$OUT"
printf 'affects routing: no\n'
