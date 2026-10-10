#!/usr/bin/env python3
from __future__ import annotations

import base64
import csv
import gzip
import importlib.util
from pathlib import Path
import sys
import tempfile

MODULE_PATH = Path(__file__).with_name("player_spawn_evidence.py")
spec = importlib.util.spec_from_file_location("player_spawn_evidence", MODULE_PATH)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


def write_gzip_tsv(path: Path, fieldnames: list[str], rows: list[dict[str, str]]) -> None:
    with gzip.open(path, "wt", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def write_tsv(path: Path, fieldnames: list[str], rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def encoded(value: str) -> str:
    return base64.b64encode(value.encode("utf-8")).decode("ascii")


def build_fixture(root: Path) -> tuple[Path, Path]:
    index = root / "index"
    index.mkdir()
    game = root / "game.tsv"

    write_gzip_tsv(
        index / "libcocos2dcpp.functions.tsv.gz",
        ["entry", "name", "address_count", "is_external", "signature"],
        [
            {
                "entry": "00021000",
                "name": "GameScene::loadGameSceneList",
                "address_count": "64",
                "is_external": "false",
                "signature": "void GameScene::loadGameSceneList(char const*)",
            },
            {
                "entry": "00022001",
                "name": "GameScene::adjustPlayerPos",
                "address_count": "16",
                "is_external": "false",
                "signature": "void GameScene::adjustPlayerPos(int)",
            },
            {
                "entry": "00023000",
                "name": "GameScene::enterScene",
                "address_count": "32",
                "is_external": "false",
                "signature": "void GameScene::enterScene()",
            },
        ],
    )
    write_gzip_tsv(
        index / "libcocos2dcpp.strings.tsv.gz",
        ["address", "utf8_base64", "length"],
        [
            {"address": "00030001", "utf8_base64": encoded("playerStartXPos"), "length": "15"},
            {"address": "00030010", "utf8_base64": encoded("playerStartYPos"), "length": "15"},
        ],
    )
    write_gzip_tsv(
        index / "libcocos2dcpp.xrefs.tsv.gz",
        ["from", "to", "type", "operand", "source"],
        [
            {"from": "00021020", "to": "00030001", "type": "DATA", "operand": "1", "source": "ANALYSIS"},
            {"from": "00021030", "to": "00030010", "type": "DATA", "operand": "1", "source": "ANALYSIS"},
        ],
    )
    write_gzip_tsv(
        index / "libcocos2dcpp.calls.tsv.gz",
        ["caller_entry", "call_site", "callee_entry"],
        [
            {"caller_entry": "00023000", "call_site": "00023010", "callee_entry": "00022001"},
        ],
    )
    write_tsv(
        game,
        ["value_hex", "code_offset_hex", "size", "thumb", "name", "demangled"],
        [
            {
                "value_hex": "0x00011001",
                "code_offset_hex": "0x00011000",
                "size": "256",
                "thumb": "1",
                "name": "load",
                "demangled": "GameScene::loadGameSceneList(char const*)",
            },
            {
                "value_hex": "0x00012001",
                "code_offset_hex": "0x00012000",
                "size": "32",
                "thumb": "1",
                "name": "adjust",
                "demangled": "GameScene::adjustPlayerPos(int)",
            },
            {
                "value_hex": "0x00013001",
                "code_offset_hex": "0x00013000",
                "size": "64",
                "thumb": "1",
                "name": "enter",
                "demangled": "GameScene::enterScene()",
            },
        ],
    )
    return index, game


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        index, game = build_fixture(Path(temp_dir))

        delta = module.derive_load_delta(index, game)
        assert delta == 0x10000

        strings = module.spawn_string_addresses(index)
        assert strings["playerStartXPos"] == 0x30001
        assert strings["playerStartYPos"] == 0x30010

        refs = module.spawn_references(index, game, delta)
        assert len(refs) == 2
        assert refs[0].from_elf == 0x11020
        assert refs[0].containing_names == ("GameScene::loadGameSceneList(char const*)",)
        assert refs[1].from_elf == 0x11030

        callers = module.adjust_player_callers(index)
        assert len(callers) == 1
        assert callers[0].caller_name == "GameScene::enterScene"
        assert callers[0].call_site == 0x23010
        assert callers[0].callee_entry == 0x22000

        report = module.render_report(index, game)
        assert "ghidra_load_delta\t0x00010000" in report
        assert "playerStartXPos\t0x00030001" in report
        assert "GameScene::loadGameSceneList(char const*)" in report
        assert "GameScene::enterScene" in report

    print("player spawn evidence smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
