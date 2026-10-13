#!/usr/bin/env python3
"""Dependency-light contract/smoke test for bounded dispatch target runtime tracing."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
JAVA = ROOT / "tools/unidbg/src/main/java/nevergone/DispatchTargetRuntimeTrace.java"
HARNESS = ROOT / "tools/unidbg/src/main/java/nevergone/NevergoneHarness.java"
BUILDER = ROOT / "tools/ml/build_nevergone_dispatch_runtime_probe_plan.py"
CONVERTER = ROOT / "tools/ml/convert_nevergone_dispatch_runtime_trace_to_seeds.py"


def require(text: str, token: str, where: str) -> None:
    if token not in text:
        raise AssertionError(f"missing {token!r} in {where}")


def main() -> int:
    java = JAVA.read_text(encoding="utf-8")
    harness = HARNESS.read_text(encoding="utf-8")
    for token in (
        "addBreakPoint",
        '"blx".equals(instruction.getMnemonic())',
        "ARM_OP_REG",
        "LIBCOCOS_GHIDRA_BIAS = 0x10000L",
        'PLAN_FORMAT = "NEVERGONE.DispatchRuntimeProbePlan/2"',
        "Stale/unsupported dispatch runtime plan",
        "Inconsistent Ghidra/runtime call-site mapping",
        "target_runtime_id",
        "target_ghidra_address",
        "target_id",
        "MAX_SITES = 128",
    ):
        require(java, token, str(JAVA))
    if ".callFunction(" in java:
        raise AssertionError("runtime tracer must observe only and never call native functions")
    require(harness, '"--trace-dispatch-targets"', str(HARNESS))
    require(harness, "DispatchTargetRuntimeTrace.install", str(HARNESS))
    if harness.index("DispatchTargetRuntimeTrace.install") > harness.index("if (probeEgl)"):
        raise AssertionError("runtime trace must be installed before workload dispatch")

    subprocess.run([sys.executable, "-m", "py_compile", str(BUILDER), str(CONVERTER)], check=True)

    with tempfile.TemporaryDirectory() as raw_tmp:
        tmp = Path(raw_tmp)
        template = tmp / "template.jsonl"
        plan = tmp / "plan.json"
        trace = tmp / "trace.log"
        seeds = tmp / "seeds.jsonl"
        template_rows = [
            {
                "source_id": "libcocos2dcpp.so:00011000",
                "call_index": 0,
                "call_address": "00011010",
                "slot_group_id": "shapeA@0x4",
                "ordered_position": 0,
                "ordered_group_id": "ordered_sequence:test",
                "ordered_group_size": 2,
            },
            {
                "source_id": "libcocos2dcpp.so:00012000",
                "call_index": 0,
                "call_address": "00012010",
                "slot_group_id": "shapeA@0x4",
                "ordered_position": 0,
                "ordered_group_id": "ordered_sequence:test",
                "ordered_group_size": 2,
            },
        ]
        template.write_text("".join(json.dumps(row) + "\n" for row in template_rows), encoding="utf-8")
        subprocess.run([sys.executable, str(BUILDER), str(template), "--output", str(plan)], check=True)
        built = json.loads(plan.read_text(encoding="utf-8"))
        assert len(built) == 2
        assert {row["format"] for row in built} == {"NEVERGONE.DispatchRuntimeProbePlan/2"}
        assert {row["call_site_offset"] for row in built} == {"0x1010", "0x2010"}
        assert {row["ghidra_address_bias"] for row in built} == {"0x10000"}
        assert {row["ghidra_call_site_address"] for row in built} == {"0x11010", "0x12010"}
        for row in built:
            ghidra = int(row["ghidra_call_site_address"], 0)
            bias = int(row["ghidra_address_bias"], 0)
            runtime = int(row["call_site_offset"], 0)
            assert ghidra - bias == runtime

        observations = [
            {
                "kind": "dispatch_runtime_target",
                "source_id": "libcocos2dcpp.so:00011000",
                "call_index": 0,
                "call_address": "00011010",
                "slot_group_id": "shapeA@0x4",
                "ordered_position": 0,
                "ordered_group_id": "ordered_sequence:test",
                "ordered_group_size": 2,
                "target_id": "libcocos2dcpp.so:0001aaaa",
                "target_module": "libcocos2dcpp.so",
                "target_module_offset": "0xaaaa",
                "target_thumb": True,
            },
            {
                "kind": "dispatch_runtime_target",
                "source_id": "libcocos2dcpp.so:00011000",
                "call_index": 0,
                "call_address": "00011010",
                "slot_group_id": "shapeA@0x4",
                "ordered_position": 0,
                "ordered_group_id": "ordered_sequence:test",
                "ordered_group_size": 2,
                "target_id": "libcocos2dcpp.so:0001bbbb",
                "target_module": "libcocos2dcpp.so",
                "target_module_offset": "0xbbbb",
                "target_thumb": True,
            },
        ]
        trace.write_text("loader noise\n" + "\n".join("EVIDENCE " + json.dumps(row) for row in observations) + "\n", encoding="utf-8")
        subprocess.run([sys.executable, str(CONVERTER), str(trace), "--output", str(seeds)], check=True)
        converted = [json.loads(line) for line in seeds.read_text(encoding="utf-8").splitlines() if line.strip()]
        assert len(converted) == 2
        assert all(row["verified"] is True for row in converted)
        assert {row["target_id"] for row in converted} == {
            "libcocos2dcpp.so:0001aaaa",
            "libcocos2dcpp.so:0001bbbb",
        }

    print("dispatch target runtime trace contract: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
