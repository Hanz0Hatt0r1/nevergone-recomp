#!/usr/bin/env bash
set -euo pipefail

: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${PLAN:=$REPO_ROOT/tools/ml/nevergone_receiver_provenance_seed_plan.tsv}"

ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
SCRIPT_PATH="$REPO_ROOT/tools/ghidra"
EVIDENCE_ROOT="$WORK_ROOT/evidence"
SLICE_DIR="$EVIDENCE_ROOT/indirect-slices"
SEED_ANALYSIS="$WORK_ROOT/seed-analysis.jsonl"
ANALYSIS="$WORK_ROOT/analysis-62calls.jsonl"
VTABLE_SLOTS="$WORK_ROOT/vtable-slots.tsv.gz"
CATALOG="$WORK_ROOT/static-vtable-slot-catalog.jsonl"
RECEIVER_TYPES="$WORK_ROOT/indirect-receiver-types.tsv"
V2="$WORK_ROOT/static-receiver-vtable-inference-v2.jsonl"
V2Q="$WORK_ROOT/static-receiver-vtable-queries-v2.jsonl"
PARAMETER_PROVENANCE="$WORK_ROOT/receiver-parameter-provenance.tsv"
PARAMETER_AUDIT_DETAILS="$WORK_ROOT/receiver-parameter-provenance-audit.jsonl"
PARAMETER_AUDIT_TXT="$WORK_ROOT/receiver-parameter-provenance-audit.txt"
V3="$WORK_ROOT/static-receiver-vtable-inference-v3.jsonl"
V3Q="$WORK_ROOT/static-receiver-vtable-queries-v3.jsonl"
STRUCTURAL_PROVENANCE="$WORK_ROOT/structural-receiver-parameter-provenance.tsv"
STRUCTURAL_AUDIT_DETAILS="$WORK_ROOT/structural-receiver-parameter-provenance-audit.jsonl"
STRUCTURAL_AUDIT_TXT="$WORK_ROOT/structural-receiver-parameter-provenance-audit.txt"
V4="$WORK_ROOT/static-receiver-vtable-inference-v4.jsonl"
V4Q="$WORK_ROOT/static-receiver-vtable-queries-v4.jsonl"
V3_FAILURES="$WORK_ROOT/receiver-type-failures-v3.jsonl"
V3_FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v3.txt"
V4_FAILURES="$WORK_ROOT/receiver-type-failures-v4.jsonl"
V4_FAILURE_AUDIT="$WORK_ROOT/receiver-type-failure-audit-v4.txt"

[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$PLAN" ]] || { echo "missing persistent seed plan: $PLAN" >&2; exit 2; }
for required in \
  ExportVtableSlots.java \
  ExportIndirectCallSlice.java \
  ExportIndirectCallReceiverTypes.java \
  ExportIndirectCallReceiverParameterProvenance.java \
  ExportIndirectCallStructuralReceiverParameterProvenance.java; do
  [[ -f "$SCRIPT_PATH/$required" ]] || { echo "missing Ghidra script: $SCRIPT_PATH/$required" >&2; exit 2; }
done

mkdir -p "$WORK_ROOT"
rm -rf "$EVIDENCE_ROOT"
mkdir -p "$SLICE_DIR"

python3 - "$PLAN" "$SEED_ANALYSIS" <<'PY'
import csv, json, sys
from pathlib import Path
plan = Path(sys.argv[1])
out = Path(sys.argv[2])
rows = list(csv.DictReader(plan.open(encoding="utf-8", newline=""), delimiter="\t"))
functions = len(rows)
calls = sum(int(row["expected_calls"]) for row in rows)
if functions != 19 or calls != 62:
    raise SystemExit(f"unexpected seed plan: functions={functions} calls={calls}; expected 19/62")
with out.open("w", encoding="utf-8") as handle:
    for row in rows:
        handle.write(json.dumps({"id": row["source_id"], "final_action": "review"}, sort_keys=True) + "\n")
print("seed plan functions:", functions)
print("seed plan calls:", calls)
print("seed analysis:", out)
PY

echo
echo "== stage 1: rebuild static catalog, receiver types, and CALLIND slices from Ghidra =="
cmd=(
  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME"
  -process "$GHIDRA_PROGRAM"
  -recursive
  -readOnly
  -noanalysis
  -scriptPath "$SCRIPT_PATH"
  -postScript ExportVtableSlots.java "$VTABLE_SLOTS"
  -postScript ExportIndirectCallReceiverTypes.java "$PLAN" "$RECEIVER_TYPES"
)
while IFS=$'\t' read -r source_id source_entry expected_calls; do
  [[ "$source_id" == "source_id" ]] && continue
  [[ -n "$source_id" && -n "$source_entry" ]] || continue
  safe_id="${source_id//:/_}"
  cmd+=(
    -postScript ExportIndirectCallSlice.java "$source_entry" "$SLICE_DIR/$safe_id.tsv" 96 24
  )
done < "$PLAN"
"${cmd[@]}"

python3 "$REPO_ROOT/tools/ml/build_nevergone_static_vtable_slot_catalog_from_export.py" \
  "$VTABLE_SLOTS" \
  --output "$CATALOG"

python3 "$REPO_ROOT/tools/ml/summarize_nevergone_indirect_call_slices.py" \
  "$SEED_ANALYSIS" \
  "$EVIDENCE_ROOT" \
  --output "$ANALYSIS"

python3 - "$ANALYSIS" <<'PY'
import json, sys
rows = [json.loads(line) for line in open(sys.argv[1], encoding="utf-8") if line.strip()]
present = [row for row in rows if isinstance(row.get("indirect_call_slice_evidence"), dict) and row["indirect_call_slice_evidence"].get("present")]
calls = sum(int(row["indirect_call_slice_evidence"].get("call_count", 0)) for row in present)
complete = sum(int(row["indirect_call_slice_evidence"].get("complete_call_count", 0)) for row in present)
budget = sum(int(row["indirect_call_slice_evidence"].get("budget_truncated_call_count", 0)) for row in present)
depth = sum(int(row["indirect_call_slice_evidence"].get("depth_truncated_call_count", 0)) for row in present)
print("rebuilt analysis functions:", len(rows))
print("slice evidence present:", len(present))
print("CALLIND calls:", calls)
print("complete calls:", complete)
print("budget truncated:", budget)
print("depth truncated:", depth)
if len(rows) != 19 or len(present) != 19 or calls != 62 or complete != 62 or budget or depth:
    raise SystemExit("fresh analysis does not reproduce the validated 19-function/62-call complete slice set")
PY

infer_base=(
  python3 "$REPO_ROOT/tools/ml/infer_nevergone_static_receiver_vtables.py"
  "$ANALYSIS" "$RECEIVER_TYPES" "$CATALOG"
)
runtime_args=()
if [[ -n "${RUNTIME_OBSERVATIONS:-}" ]]; then
  [[ -f "$RUNTIME_OBSERVATIONS" ]] || { echo "missing runtime observations: $RUNTIME_OBSERVATIONS" >&2; exit 2; }
  runtime_args=(--runtime-observations "$RUNTIME_OBSERVATIONS")
fi

echo
echo "== stage 1b: baseline receiver inference =="
"${infer_base[@]}" \
  "${runtime_args[@]}" \
  --output "$V2" \
  --queries-output "$V2Q"

echo
echo "== stage 2: direct receiver LOAD-address parameter/caller provenance =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$SCRIPT_PATH" \
  -postScript ExportIndirectCallReceiverParameterProvenance.java "$PLAN" "$PARAMETER_PROVENANCE"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_parameter_provenance.py" \
  "$V2" "$PARAMETER_PROVENANCE" "$CATALOG" \
  --details-output "$PARAMETER_AUDIT_DETAILS" --samples 60 \
  | tee "$PARAMETER_AUDIT_TXT"

"${infer_base[@]}" \
  --parameter-provenance "$PARAMETER_PROVENANCE" \
  "${runtime_args[@]}" \
  --output "$V3" \
  --queries-output "$V3Q"

echo
echo "== stage 3: structurally recovered receiver LOAD-address caller provenance =="
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$SCRIPT_PATH" \
  -postScript ExportIndirectCallStructuralReceiverParameterProvenance.java "$PLAN" "$STRUCTURAL_PROVENANCE"

python3 "$REPO_ROOT/tools/ml/audit_nevergone_structural_receiver_parameter_provenance.py" \
  "$V3" "$STRUCTURAL_PROVENANCE" "$CATALOG" \
  --details-output "$STRUCTURAL_AUDIT_DETAILS" --samples 60 \
  | tee "$STRUCTURAL_AUDIT_TXT"

echo
echo "== stage 4: structural-caller-aware production inference =="
"${infer_base[@]}" \
  --parameter-provenance "$PARAMETER_PROVENANCE" \
  --structural-parameter-provenance "$STRUCTURAL_PROVENANCE" \
  "${runtime_args[@]}" \
  --output "$V4" \
  --queries-output "$V4Q"

echo
echo "caller-aware v3 failure audit:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" "$V3" \
  --details-output "$V3_FAILURES" --samples 0 \
  | tee "$V3_FAILURE_AUDIT"

echo
echo "structural-caller-aware v4 failure audit:"
python3 "$REPO_ROOT/tools/ml/audit_nevergone_receiver_type_failures.py" \
  "$RECEIVER_TYPES" "$V4" \
  --details-output "$V4_FAILURES" --samples 60 \
  | tee "$V4_FAILURE_AUDIT"

echo
echo "== persistent outputs =="
printf 'work root: %s\n' "$WORK_ROOT"
printf 'analysis: %s\n' "$ANALYSIS"
printf 'catalog: %s\n' "$CATALOG"
printf 'receiver types: %s\n' "$RECEIVER_TYPES"
printf 'parameter provenance: %s\n' "$PARAMETER_PROVENANCE"
printf 'structural provenance: %s\n' "$STRUCTURAL_PROVENANCE"
printf 'v3 inference: %s\n' "$V3"
printf 'v4 inference: %s\n' "$V4"
printf 'v4 queries: %s\n' "$V4Q"
printf 'v4 failure audit: %s\n' "$V4_FAILURE_AUDIT"
printf 'affects routing: no\n'
