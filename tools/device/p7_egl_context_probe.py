#!/usr/bin/env python3
"""Collect bounded P7 evidence for EGL context and debug-package state.

This deliberately does not assume that every libEGL 'no current context' line is
caused by first-scene texture upload. It records package/debuggability state and
extracts app-scoped logcat lines so the render-thread hypothesis can be tested
against one device run.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
from dataclasses import dataclass

DEFAULT_PACKAGE = "org.nevergone.recomp"
NO_CONTEXT_RE = re.compile(r"call to OpenGL ES API with no current context", re.IGNORECASE)
INTERESTING_RE = re.compile(
    r"(?:NeverGone|libEGL|EGL|GameScene|RenderQueue|assetStore|GLES2)",
    re.IGNORECASE,
)


@dataclass
class CommandResult:
    argv: list[str]
    returncode: int
    stdout: str
    stderr: str


def run(argv: list[str], timeout: float = 15.0) -> CommandResult:
    try:
        completed = subprocess.run(
            argv,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
        return CommandResult(argv, completed.returncode, completed.stdout, completed.stderr)
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout if isinstance(error.stdout, str) else ""
        stderr = error.stderr if isinstance(error.stderr, str) else ""
        return CommandResult(argv, 124, stdout, stderr + "\ncommand timed out")


def adb_prefix(serial: str | None) -> list[str]:
    prefix = ["adb"]
    if serial:
        prefix += ["-s", serial]
    return prefix


def parse_debuggable(dumpsys: str) -> bool | None:
    upper = dumpsys.upper()
    flag_blocks = re.findall(r"(?:FLAGS|PKGFLAGS)=\[([^\]]*)\]", upper)
    if flag_blocks:
        return any("DEBUGGABLE" in block for block in flag_blocks)
    if "DEBUGGABLE" in upper:
        return True
    return None


def tail_lines(text: str, limit: int) -> list[str]:
    lines = [line.rstrip() for line in text.splitlines() if line.strip()]
    return lines[-limit:]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device-id", help="adb serial; default uses adb's selected device")
    parser.add_argument("--package", default=DEFAULT_PACKAGE)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("docs/evidence/p7-egl-context"))
    parser.add_argument("--log-lines", type=int, default=200, help="Maximum retained interesting log lines")
    parser.add_argument(
        "--require-clean-context",
        action="store_true",
        help="Exit 1 when app-scoped logcat contains a no-current-context EGL error.",
    )
    args = parser.parse_args()

    prefix = adb_prefix(args.device_id)
    devices = run(["adb", "devices"])
    if devices.returncode != 0:
        print(devices.stderr or devices.stdout, file=sys.stderr)
        return 2

    package_path = run(prefix + ["shell", "pm", "path", args.package])
    installed = package_path.returncode == 0 and "package:" in package_path.stdout

    dumpsys = run(prefix + ["shell", "dumpsys", "package", args.package]) if installed else CommandResult([], 1, "", "package not installed")
    debuggable = parse_debuggable(dumpsys.stdout) if installed else None

    run_as = run(prefix + ["shell", "run-as", args.package, "sh", "-c", "pwd && ls -ld files 2>/dev/null || true"]) if installed else CommandResult([], 1, "", "package not installed")
    run_as_ok = run_as.returncode == 0

    pid_result = run(prefix + ["shell", "pidof", "-s", args.package]) if installed else CommandResult([], 1, "", "package not installed")
    pid = pid_result.stdout.strip().split()[0] if pid_result.returncode == 0 and pid_result.stdout.strip() else None

    logcat_argv = prefix + ["logcat", "-d", "-v", "threadtime"]
    if pid:
        logcat_argv += ["--pid", pid]
    logcat = run(logcat_argv, timeout=25.0)
    interesting = [line for line in logcat.stdout.splitlines() if INTERESTING_RE.search(line)]
    retained = interesting[-max(1, args.log_lines):]
    no_context_lines = [line for line in retained if NO_CONTEXT_RE.search(line)]

    args.out.mkdir(parents=True, exist_ok=True)
    result = {
        "package": args.package,
        "installed": installed,
        "pid": pid,
        "logcat_pid_scoped": bool(pid),
        "debuggable": debuggable,
        "run_as_ok": run_as_ok,
        "run_as_returncode": run_as.returncode,
        "run_as_stdout": run_as.stdout.strip(),
        "run_as_stderr": run_as.stderr.strip(),
        "no_current_context_count": len(no_context_lines),
        "interesting_log_lines": retained,
    }
    (args.out / "probe.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    report = [
        "# P7 EGL context probe",
        "",
        f"- package: `{args.package}`",
        f"- installed: `{installed}`",
        f"- pid: `{pid}`",
        f"- logcat scoped to app pid: `{bool(pid)}`",
        f"- debuggable: `{debuggable}`",
        f"- run-as works: `{run_as_ok}`",
        f"- retained `no current context` lines: `{len(no_context_lines)}`",
        "",
        "## Interpretation",
        "",
    ]
    if not installed:
        report.append("- The requested package is not installed. Verify the package id before interpreting `run-as` errors.")
    elif debuggable is False:
        report.append("- The installed package is not marked debuggable; `run-as` failure is expected for that installed APK.")
    elif debuggable is True and not run_as_ok:
        report.append("- The package is marked debuggable but `run-as` still failed; retain the stderr below as a separate device/environment issue.")
    elif run_as_ok:
        report.append("- `run-as` succeeded, so app-private files can be inspected without changing the manifest.")
    else:
        report.append("- Debuggability could not be proven from `dumpsys`; inspect the retained package output on-device if needed.")

    if not pid:
        report.append("- The app process is not currently running, so retained logcat is not PID-scoped and cannot by itself attribute EGL errors to Nevergone.")
    elif no_context_lines:
        report.append("- PID-scoped EGL no-current-context calls are present. This proves the Nevergone process made an invalid GL call, but not which caller/thread caused it.")
    else:
        report.append("- No EGL no-current-context line is present in the app PID-scoped log window.")

    if run_as.stderr.strip():
        report.extend(["", "## run-as stderr", "", "```text", *tail_lines(run_as.stderr, 40), "```"])
    if retained:
        report.extend(["", "## Relevant logcat", "", "```text", *retained, "```"])

    (args.out / "report.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    print(
        f"package={args.package} installed={installed} pid={pid} debuggable={debuggable} "
        f"run_as={run_as_ok} no_current_context={len(no_context_lines)} "
        f"report={args.out / 'report.md'}"
    )

    if not installed:
        return 2
    if args.require_clean_context and pid and no_context_lines:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
