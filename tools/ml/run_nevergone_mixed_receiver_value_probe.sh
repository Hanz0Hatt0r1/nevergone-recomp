#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${MAX_NODES:=128}"
: "${MAX_DEPTH:=20}"

INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v11.jsonl"
SLICES="$WORK_ROOT/evidence/mixed-callind-receiver-002a7594.tsv"
DETAILS="$WORK_ROOT/mixed-callind-receiver-value-audit.jsonl"
LOG="$WORK_ROOT/mixed-callind-receiver-value-audit.txt"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in "$INFERENCE" \
  "$REPO_ROOT/tools/ml/audit_nevergone_mixed_receiver_value_probe.py" \
  "$REPO_ROOT/tools/ghidra/ExportIndirectCallStructuralReceiverSlice.java"; do
  [[ -f "$required" ]] || { echo "missing required input: $required" >&2; exit 2; }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
mkdir -p "$(dirname "$SLICES")"

# Check that v11 has been regenerated with the quarantine fix BEFORE
# launching Ghidra; stale "resolved_converged_target" must be rejected.
python3 - "$INFERENCE" <<'PY'
import json, sys
source = "libcocos2dcpp.so:002a7594"
rows = [json.loads(line) for line in open(sys.argv[1], encoding="utf-8") if line.strip()]
selected = {row["call_index"]: row for row in rows if row.get("source_id") == source
            and row.get("call_index") in (3, 4, 7)}
if set(selected) != {3, 4, 7} or any(
        row.get("status") != "receiver_not_single_pointer"
        or row.get("dispatch_slot_offset") != "0x80" for row in selected.values()):
    raise SystemExit("stale v11: first run bash tools/ml/run_nevergone_mixed_slot_target_recovery_v11.sh")
print("v11 quarantined receiver identities: 3/3")
PY

"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallStructuralReceiverSlice.java \
  002a7594 "$SLICES" "$MAX_NODES" "$MAX_DEPTH"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_mixed_receiver_value_probe.py" \
  "$INFERENCE" "$SLICES" --details-output "$DETAILS" --require-complete | tee "$LOG"

printf '\nraw receiver slices: %s\naudit: %s\nlog: %s\naffects routing: no\n' \
  "$SLICES" "$DETAILS" "$LOG"
