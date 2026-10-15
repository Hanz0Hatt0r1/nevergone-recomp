#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <original-apk-or-elf> <controlsbutton-owner-field-evidence.tar.gz>" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ORIGINAL="$1"
OWNER_ARCHIVE="$2"
: "${WORK_ROOT:=$HOME/.cache/nevergone-recomp/receiver-provenance}"
: "${OUT_ARCHIVE:=$HOME/nevergone-controlsbutton-owner-first-hop.tar.gz}"

DETAILS="$WORK_ROOT/controlsbutton-owner-first-hop-audit.jsonl"
TEXT="$WORK_ROOT/controlsbutton-owner-first-hop-audit.txt"
mkdir -p "$WORK_ROOT"

python3 "$ROOT/tools/ml/audit_nevergone_controlsbutton_owner_first_hop.py" \
  "$ORIGINAL" "$OWNER_ARCHIVE" --details-output "$DETAILS" | tee "$TEXT"

python3 - "$DETAILS" <<'PY'
import json, sys
from pathlib import Path
p = Path(sys.argv[1])
row = json.loads(p.read_text(encoding="utf-8"))
assert row["format"] == "NEVERGONE.ControlsButtonOwnerFieldFirstHop/1"
assert row["status"] == "controlsbutton_owner_first_hop_classified"
assert row["target_fields_written_by_first_hop_external_targets"] is False
assert row["owner_fields_stable"] is False
assert row["receiver_class_proven"] is False
assert row["affects_routing"] is False
assert row["full_this_edge_count"] == 23
assert row["internal_controlsbutton_edge_count"] == 8
assert row["external_full_this_edge_count"] == 15
assert row["external_direct_edge_count"] == 5
assert row["external_indirect_edge_count"] == 10
PY

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cp "$DETAILS" "$tmp/"
cp "$TEXT" "$tmp/"
tar -czf "$OUT_ARCHIVE" -C "$tmp" .

echo
echo "Archive: $OUT_ARCHIVE"
echo "Original binary included: no"
echo "Owner evidence archive included: no"
echo "Affects routing: no"
