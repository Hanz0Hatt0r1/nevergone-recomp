#!/usr/bin/env python3

from __future__ import annotations

import base64
import csv
import gzip
import subprocess
import sys
import tempfile
from pathlib import Path


def write_tsv(path: Path, header: list[str], rows: list[list[str]]) -> None:
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(header)
        writer.writerows(rows)


def run(tool: Path, root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(tool), *args, "--root", str(root)],
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    tool = repo / "tools/ghidra/query_full_index.py"
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        write_tsv(
            root / "libcocos2dcpp.functions.tsv.gz",
            ["entry", "name", "address_count", "is_external", "signature"],
            [
                ["00435a40", "SingleLoginLayer::InitUI", "500", "false", "void InitUI(void)"],
                ["004255d8", "SingleLoginLayer::Func01", "100", "false", "void Func01(void)"],
            ],
        )
        write_tsv(
            root / "libcocos2dcpp.strings.tsv.gz",
            ["address", "utf8_base64", "length"],
            [["0078c78f", base64.b64encode(b"zjmshandian01.png").decode("ascii"), "17"]],
        )
        write_tsv(
            root / "libcocos2dcpp.symbols.tsv.gz",
            ["address", "name", "type", "source", "is_primary"],
            [["00435a40", "SingleLoginLayer::InitUI", "Function", "USER_DEFINED", "true"]],
        )
        write_tsv(
            root / "libcocos2dcpp.xrefs.tsv.gz",
            ["from", "to", "type", "operand", "source"],
            [["004368d2", "0078c78f", "DATA", "1", "ANALYSIS"]],
        )
        write_tsv(
            root / "libcocos2dcpp.calls.tsv.gz",
            ["caller_entry", "call_site", "callee_entry"],
            [["00435a40", "00435b10", "004255d8"]],
        )
        write_tsv(
            root / "archive_entries.tsv.gz",
            ["archive", "path", "size", "compressed_size", "crc32"],
            [["apk", "assets/gamescene_ui/SingleLogin_UI/SingleLogin_default.png", "123", "100", "deadbeef"]],
        )
        write_tsv(
            root / "atlas_frames.tsv.gz",
            ["archive", "plist", "frame", "x", "y", "width", "height", "offset_x", "offset_y", "source_width", "source_height", "rotated"],
            [["apk", "SingleLogin_default.plist", "zjmshandian01.png", "0", "0", "10", "20", "0", "0", "1136", "640", "false"]],
        )

        result = run(tool, root, "functions", "singlelogin")
        assert result.returncode == 0 and "SingleLoginLayer::InitUI" in result.stdout

        result = run(tool, root, "strings", "shandian")
        assert result.returncode == 0 and "zjmshandian01.png" in result.stdout

        result = run(tool, root, "callers", "Func01")
        assert result.returncode == 0 and "SingleLoginLayer::InitUI" in result.stdout

        result = run(tool, root, "callees", "00435a40")
        assert result.returncode == 0 and "SingleLoginLayer::Func01" in result.stdout

        result = run(tool, root, "atlases", "zjmshandian01")
        assert result.returncode == 0 and "1136" in result.stdout

        result = run(tool, root, "assets", "SingleLogin_default.png")
        assert result.returncode == 0 and "SingleLogin_default.png" in result.stdout

        result = run(tool, root, "functions", "does-not-exist")
        assert result.returncode == 1

    print("query_full_index smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
