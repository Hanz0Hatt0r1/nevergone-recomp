#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "tools/unidbg/src/main/java/nevergone/ActionDataCreateProbe.java"
FALLBACK = ROOT / "tools/unidbg/src/main/java/nevergone/ActionDataFallbackPathControl.java"
HARNESS = ROOT / "tools/unidbg/src/main/java/nevergone/NevergoneHarness.java"


def require(text: str, token: str, where: str) -> None:
    if token not in text:
        raise AssertionError(f"missing {token!r} in {where}")


def main() -> int:
    probe = PROBE.read_text(encoding="utf-8")
    fallback = FALLBACK.read_text(encoding="utf-8")
    harness = HARNESS.read_text(encoding="utf-8")

    for token in (
        "GHIDRA_BIAS = 0x10000L",
        "GHIDRA_CREATE_A = 0x29ec10L",
        "GHIDRA_CREATE_B = 0x29ed0cL",
        "runtimeOffset = ghidraAddress - GHIDRA_BIAS",
        "module.callFunction(emulator, runtimeOffset | 1L)",
        '"guest_argument_count", 0',
        '"ghidra_address"',
        '"runtime_offset"',
        '"action_data_create_entry"',
        '"action_data_create_preflight_disassembly"',
        '"expected_dispatch_sites"',
        '"instruction_boundary"',
        '"is_blx"',
        "0x29ec26L",
        "0x29ec3cL",
        "0x29ed22L",
        "0x29ed38L",
        "PREFLIGHT_BYTES = 0x80",
        "PREFLIGHT_MAX_INSTRUCTIONS = 64",
        'FALLBACK_SUFFIX = ":fallback"',
        "ActionDataFallbackPathControl.install",
        '"fallback_path_requested"',
        '"post_first_virtual_call_r0_zero_only"',
    ):
        require(probe, token, str(PROBE))

    for token in (
        "GHIDRA_CREATE_A = 0x29ec10L",
        "GHIDRA_CREATE_B = 0x29ed0cL",
        "GHIDRA_FIRST_CALL_A = 0x29ec26L",
        "GHIDRA_FIRST_CALL_B = 0x29ed22L",
        "postCallGhidra = firstCallGhidra + 2L",
        "debugger.addBreakPoint((module.base + postCallOffset) | 1L",
        "reg_read(ArmConst.UC_ARM_REG_R0)",
        "reg_write(ArmConst.UC_ARM_REG_R0, 0)",
        '"action_data_fallback_return_override"',
        '"r0_only_after_first_virtual_call_return"',
        '"first_virtual_call_executed_normally", true',
        '"call_target_modified", false',
        '"receiver_memory_modified", false',
    ):
        require(fallback, token, str(FALLBACK))

    for forbidden in (
        "mem_write(",
        ".callFunction(",
        "UC_ARM_REG_PC",
        "UC_ARM_REG_LR",
        "UC_ARM_REG_SP",
    ):
        if forbidden in fallback:
            raise AssertionError(f"fallback control must not contain {forbidden!r}")

    if "module.callFunction(emulator, runtimeOffset | 1L," in probe:
        raise AssertionError("create(void) probe must not pass guest arguments")
    if "0x2ace4c" in probe.lower() or "0x2ace4c" in fallback.lower():
        raise AssertionError("higher-risk 0x2ace4c must not be in direct-entry/fallback allowlist")

    require(harness, '"--probe-action-data-create"', str(HARNESS))
    require(harness, "ActionDataCreateProbe.run", str(HARNESS))
    require(harness, "DispatchTargetRuntimeTrace.install", str(HARNESS))
    if harness.index("DispatchTargetRuntimeTrace.install") > harness.index("ActionDataCreateProbe.run"):
        raise AssertionError("dispatch tracer must be installed before direct-entry workload")

    print("action data create/fallback probe contract: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
