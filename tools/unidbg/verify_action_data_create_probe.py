#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "tools/unidbg/src/main/java/nevergone/ActionDataCreateProbe.java"
HARNESS = ROOT / "tools/unidbg/src/main/java/nevergone/NevergoneHarness.java"


def require(text: str, token: str, where: str) -> None:
    if token not in text:
        raise AssertionError(f"missing {token!r} in {where}")


def main() -> int:
    probe = PROBE.read_text(encoding="utf-8")
    harness = HARNESS.read_text(encoding="utf-8")

    for token in (
        "0x29ec10L",
        "0x29ed0cL",
        "offset != CREATE_A && offset != CREATE_B",
        "module.callFunction(emulator, offset | 1L)",
        '"guest_argument_count", 0',
        '"action_data_create_entry"',
    ):
        require(probe, token, str(PROBE))

    if "module.callFunction(emulator, offset | 1L," in probe:
        raise AssertionError("create(void) probe must not pass guest arguments")
    if "0x2ace4c" in probe.lower():
        raise AssertionError("higher-risk 0x2ace4c must not be in direct-entry allowlist")

    require(harness, '"--probe-action-data-create"', str(HARNESS))
    require(harness, "ActionDataCreateProbe.run", str(HARNESS))
    require(harness, "DispatchTargetRuntimeTrace.install", str(HARNESS))
    if harness.index("DispatchTargetRuntimeTrace.install") > harness.index("ActionDataCreateProbe.run"):
        raise AssertionError("dispatch tracer must be installed before direct-entry workload")

    print("action data create probe contract: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
