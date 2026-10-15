#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${MAX_NODES:=160}"
: "${MAX_DEPTH:=24}"

FAILURES="$WORK_ROOT/receiver-type-failures-v17.jsonl"
INFERENCE="$WORK_ROOT/static-receiver-vtable-inference-v17.jsonl"
PLAN="$WORK_ROOT/non-slot80-receiver-frontier-v18-plan.tsv"
SLICE_DIR="$WORK_ROOT/evidence/non-slot80-receiver-frontier-v18"
DETAILS="$WORK_ROOT/non-slot80-receiver-frontier-v18-audit.jsonl"
AUDIT_TXT="$WORK_ROOT/non-slot80-receiver-frontier-v18-audit.txt"
ARCHIVE="$HOME/nevergone-non-slot80-receiver-frontier-v18.tar.gz"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

for required in \
  "$FAILURES" \
  "$INFERENCE" \
  "$REPO_ROOT/tools/ml/plan_nevergone_non_slot80_receiver_frontier_v18.py" \
  "$REPO_ROOT/tools/ml/audit_nevergone_non_slot80_receiver_frontier_v18.py" \
  "$REPO_ROOT/tools/ghidra/ExportIndirectCallStructuralReceiverSlice.java"; do
  [[ -f "$required" ]] || {
    echo "missing required input: $required" >&2
    echo "Run the v17 end-to-end receiver recovery first if the v17 artifacts are absent." >&2
    exit 2
  }
done
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }

rm -rf "$SLICE_DIR"
mkdir -p "$SLICE_DIR"

python3 "$REPO_ROOT/tools/ml/plan_nevergone_non_slot80_receiver_frontier_v18.py" \
  "$FAILURES" "$INFERENCE" \
  --output "$PLAN" \
  --expected-calls 7

while IFS=$'\t' read -r format source entry calls addresses slots expected; do
  [[ "$format" == "NEVERGONE.NonSlot80ReceiverFrontierPlan/1" ]] || {
    echo "invalid v18 plan format: $format" >&2
    exit 2
  }
  [[ "$entry" =~ ^[0-9a-f]{8}$ ]] || { echo "invalid source entry: $entry" >&2; exit 2; }
  case ",$slots," in
    *,0x80,*) echo "v18 plan unexpectedly contains slot 0x80" >&2; exit 2 ;;
  esac

  safe="${source//\//_}"
  safe="${safe//:/_}"
  out="$SLICE_DIR/$safe.tsv"
  printf '\n== Ghidra v18 structural receiver: %s (calls %s; slots %s) ==\n' \
    "$source" "$calls" "$slots"
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
    -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
    -scriptPath "$REPO_ROOT/tools/ghidra" \
    -postScript ExportIndirectCallStructuralReceiverSlice.java \
    "$entry" "$out" "$MAX_NODES" "$MAX_DEPTH"
done < <(tail -n +2 "$PLAN")

python3 "$REPO_ROOT/tools/ml/audit_nevergone_non_slot80_receiver_frontier_v18.py" \
  "$PLAN" "$FAILURES" "$INFERENCE" "$SLICE_DIR" \
  --details-output "$DETAILS" \
  --require-complete | tee "$AUDIT_TXT"

python3 - "$PLAN" "$DETAILS" <<'PY'
import csv
import json
import sys

with open(sys.argv[1], encoding="utf-8", newline="") as handle:
    plan = list(csv.DictReader(handle, delimiter="\t"))
with open(sys.argv[2], encoding="utf-8") as handle:
    rows = [json.loads(line) for line in handle if line.strip()]

planned = sum(int(row["expected_calls"]) for row in plan)
if planned != 7 or len(rows) != 7:
    raise SystemExit(f"v18 residual frontier count changed: plan={planned} audit={len(rows)}")
if any(row.get("dispatch_slot_offset") == "0x80" for row in rows):
    raise SystemExit("v18 residual frontier contains slot 0x80")
if any(row.get("previous_status") != "unresolved_receiver_class" for row in rows):
    raise SystemExit("v18 residual frontier contains a new failure status")
if any(row.get("status") != "complete_structural_slice" for row in rows):
    raise SystemExit("v18 structural slice completeness gate failed")
if any(row.get("receiver_class_proven") is not False for row in rows):
    raise SystemExit("v18 must not claim a receiver class")
if any(row.get("affects_routing") is not False for row in rows):
    raise SystemExit("v18 must remain evidence-only")

print("v18 seven-call non-slot80 frontier gate: pass")
for row in rows:
    print(
        row["source_id"],
        "call", row["call_index"],
        "slot", row["dispatch_slot_offset"],
        "root", row["root_opcode"],
        "next", row["next_probe_kind"],
    )
PY

for required in \
  "$PLAN" \
  "$DETAILS" \
  "$AUDIT_TXT" \
  "$FAILURES" \
  "$INFERENCE"; do
  [[ -f "$required" ]] || { echo "missing archive member: $required" >&2; exit 2; }
done

tar -C "$WORK_ROOT" -czf "$ARCHIVE" \
  "$(basename "$PLAN")" \
  "$(basename "$DETAILS")" \
  "$(basename "$AUDIT_TXT")" \
  "$(basename "$FAILURES")" \
  "$(basename "$INFERENCE")" \
  "evidence/non-slot80-receiver-frontier-v18"

printf '\narchive: %s\nresidual calls required: 7\nslot 0x80 allowed: no\nreceiver class proven: no\naffects routing: no\n' "$ARCHIVE"
