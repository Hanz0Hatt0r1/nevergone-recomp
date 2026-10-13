#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ml/audit_nevergone_static_vtable_prefixes.py"


def main() -> int:
    subprocess.run([sys.executable, "-m", "py_compile", str(TOOL)], check=True)

    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        catalog = tmp / "catalog.jsonl"
        largest = tmp / "largest.jsonl"

        rows = []
        for cls, ap, count, base in [
            ("A", 0x1008, 4, 0x2000),
            ("B", 0x1108, 8, 0x2000),
        ]:
            for slot in range(count):
                target = base + (slot * 4 if cls == "A" else slot * 4)
                rows.append({
                    "format": "NEVERGONE.StaticVtableSlotCatalog/3",
                    "class_name": cls,
                    "receiver_vtable_id": f"libcocos2dcpp.so:{ap:08x}",
                    "receiver_vtable_address_point": f"0x{ap:x}",
                    "next_vtable_symbol_starts": [f"0x{ap + count*4 + 0x20:x}"],
                    "dispatch_slot_offset": f"0x{slot*4:x}",
                    "primary_prefix_slot_count": count,
                    "candidate_target_ids": [f"libcocos2dcpp.so:{target:08x}"],
                    "target_functions": [{"name": f"{cls}::v{slot}"}],
                    "runtime_validation_status": "runtime_match" if slot == 0 else "runtime_unobserved",
                })

        catalog.write_text("".join(json.dumps(x) + "\n" for x in rows), encoding="utf-8")
        result = subprocess.run([
            sys.executable, str(TOOL), str(catalog),
            "--largest-output", str(largest),
            "--top", "2",
        ], check=True, text=True, capture_output=True)

        out = result.stdout
        assert "format: NEVERGONE.StaticVtablePrefixAudit/1" in out
        assert "catalog rows: 12" in out
        assert "vtables: 2" in out
        assert "unique target functions: 8" in out
        assert "slot/unique-target ratio: 1.5" in out
        assert "prefix slots median: 6.0" in out
        assert "prefix slots max: 8" in out
        assert "runtime matches: 2" in out
        assert "runtime mismatches: 0" in out

        ranked = [json.loads(line) for line in largest.read_text(encoding="utf-8").splitlines() if line.strip()]
        assert ranked[0]["class_name"] == "B"
        assert ranked[0]["primary_prefix_slot_count"] == 8
        assert ranked[1]["class_name"] == "A"

    print("static vtable prefix audit smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
