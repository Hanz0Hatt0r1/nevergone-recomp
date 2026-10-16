#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools/ghidra/run_player_current_frame_probe.sh"
HEADER = (
    "function_entry\tfunction_name\tstore_address\tstore_time\tfield_offset_hex\t"
    "address_opcode\taddress_op_address\taddress_op_time\tbase_token\tvalue_token\t"
    "value_is_constant\tvalue_constant_hex\n"
)
ROW = (
    "00401000\tMeActionsSystem::play\t00401020\t1\t0x1a4\tPTRSUB\t"
    "0040101c\t0\tregister:r0:4\tconst:0x0:4\t1\t0x0\n"
)


def run_case(shared: bool) -> None:
    with tempfile.TemporaryDirectory(prefix="player-frame-wrapper-") as temp:
        root = Path(temp)
        ghidra_home = root / "ghidra"
        support = ghidra_home / "support"
        support.mkdir(parents=True)
        project = root / "project"
        project.mkdir()
        output = root / "output"
        shared_repo = root / "revenginstr"
        shared_runner = shared_repo / "scripts/run_ghidra_field_store_candidates.sh"
        trace = root / "trace.txt"
        analyzer_trace = root / "analyzer.txt"

        analyzer = support / "analyzeHeadless"
        analyzer.write_text(
            "#!/usr/bin/env bash\n"
            "set -euo pipefail\n"
            "printf '%s\\n' \"$@\" > \"$ANALYZER_TRACE\"\n"
            "out=\"${!#}\"\n"
            f"printf '%b' {HEADER!r} > \"$out\"\n"
            f"printf '%b' {ROW!r} >> \"$out\"\n",
            encoding="utf-8",
        )
        analyzer.chmod(0o755)

        if shared:
            shared_runner.parent.mkdir(parents=True)
            shared_runner.write_text(
                "#!/usr/bin/env bash\n"
                "set -euo pipefail\n"
                "printf '%s\\n' \"$@\" > \"$SHARED_TRACE\"\n"
                "out=$6\n"
                f"printf '%b' {HEADER!r} > \"$out\"\n"
                f"printf '%b' {ROW!r} >> \"$out\"\n",
                encoding="utf-8",
            )
            shared_runner.chmod(0o755)

        environment = os.environ.copy()
        environment.update({
            "REVENGINSTR_REPO": str(shared_repo),
            "SHARED_TRACE": str(trace),
            "ANALYZER_TRACE": str(analyzer_trace),
        })
        result = subprocess.run(
            ["bash", str(RUNNER), str(ghidra_home), str(project), "project-name", "program.bin", str(output)],
            check=False,
            capture_output=True,
            text=True,
            env=environment,
        )
        if result.returncode:
            raise AssertionError(f"wrapper failed: {result.stderr}\n{result.stdout}")
        summary = (output / "player-current-frame-summary.txt").read_text(encoding="utf-8")
        if "candidate_stores=1" not in summary:
            raise AssertionError("wrapper did not summarize the exported candidate")

        if shared:
            arguments = trace.read_text(encoding="utf-8").splitlines()
            expected = [str(project), "project-name", "program.bin", "MeActionsSystem", "0x1a4", str(output / "player-current-frame-stores.tsv")]
            if arguments != expected:
                raise AssertionError(f"shared runner arguments mismatch: {arguments!r}")
            if analyzer_trace.exists():
                raise AssertionError("shared runner path unexpectedly used local Ghidra exporter")
        else:
            arguments = analyzer_trace.read_text(encoding="utf-8").splitlines()
            if "ExportFieldStoreCandidates.java" not in arguments or arguments[-3:] != ["MeActionsSystem", "0x1a4", str(output / "player-current-frame-stores.tsv")]:
                raise AssertionError(f"legacy exporter fallback arguments mismatch: {arguments!r}")


def main() -> int:
    run_case(shared=True)
    run_case(shared=False)
    print("player current-frame shared-first/fallback wrapper: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
