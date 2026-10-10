#!/usr/bin/env python3
"""Correlate player-spawn strings with checked-in Ghidra and ELF metadata.

This tool reads metadata only. It never reads original binaries, assets, Ghidra
project databases, or decompiler output. Ghidra code addresses are normalized
back to ELF code offsets by deriving the load delta from a function present in
both metadata sets. Data/string addresses remain exact.
"""

from __future__ import annotations

import argparse
import base64
import csv
import gzip
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator

DEFAULT_INDEX_ROOT = Path("docs/ghidra/full-index")
DEFAULT_GAME_SYMBOLS = Path("docs/evidence/unidbg-2026-10-10/game.tsv")
PROGRAM = "libcocos2dcpp"
LOAD_FUNCTION = "GameScene::loadGameSceneList"
ADJUST_FUNCTION = "GameScene::adjustPlayerPos"
SPAWN_KEYS = ("playerStartXPos", "playerStartYPos")


def parse_address(value: str) -> int:
    text = value.strip().lower()
    if text in ("", "-"):
        raise ValueError(f"invalid address {value!r}")
    return int(text, 16)


def parse_code_address(value: str) -> int:
    # ARM Thumb function pointers may carry bit 0. Code offsets do not.
    return parse_address(value) & ~1


def hex_address(value: int) -> str:
    return f"0x{value:08x}"


def open_gzip_tsv(path: Path) -> Iterator[dict[str, str]]:
    with gzip.open(path, "rt", encoding="utf-8", newline="") as handle:
        yield from csv.DictReader(handle, delimiter="\t")


def open_tsv(path: Path) -> Iterator[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        yield from csv.DictReader(handle, delimiter="\t")


def decode_string(row: dict[str, str]) -> str:
    encoded = row.get("utf8_base64", "")
    return base64.b64decode(encoded, validate=True).decode("utf-8")


def unique_named_function(
    rows: Iterable[dict[str, str]],
    token: str,
    *,
    address_field: str,
    name_field: str,
) -> tuple[int, str]:
    matches: list[tuple[int, str]] = []
    for row in rows:
        name = row.get(name_field, "")
        if token not in name:
            continue
        matches.append((parse_code_address(row[address_field]), name))
    unique = list(dict.fromkeys(matches))
    if len(unique) != 1:
        raise ValueError(
            f"expected one {token!r} function, found {len(unique)}: "
            + ", ".join(name for _, name in unique[:5])
        )
    return unique[0]


@dataclass(frozen=True)
class FunctionRange:
    start: int
    end: int
    name: str

    def contains(self, address: int) -> bool:
        return self.start <= address < self.end


def load_elf_ranges(path: Path) -> list[FunctionRange]:
    ranges: list[FunctionRange] = []
    for row in open_tsv(path):
        start = parse_code_address(row["code_offset_hex"])
        size = int(row["size"], 10)
        if size <= 0:
            continue
        ranges.append(FunctionRange(start, start + size, row.get("demangled", "")))
    ranges.sort(key=lambda item: (item.start, item.end, item.name))
    return ranges


def containing_functions(ranges: list[FunctionRange], address: int) -> list[FunctionRange]:
    return [item for item in ranges if item.contains(address)]


def derive_load_delta(index_root: Path, game_symbols: Path) -> int:
    ghidra_entry, _ = unique_named_function(
        open_gzip_tsv(index_root / f"{PROGRAM}.functions.tsv.gz"),
        LOAD_FUNCTION,
        address_field="entry",
        name_field="name",
    )
    elf_entry, _ = unique_named_function(
        open_tsv(game_symbols),
        LOAD_FUNCTION,
        address_field="code_offset_hex",
        name_field="demangled",
    )
    delta = ghidra_entry - elf_entry
    if delta < 0:
        raise ValueError(f"negative Ghidra load delta: {delta}")
    return delta


def spawn_string_addresses(index_root: Path) -> dict[str, int]:
    matches: dict[str, list[int]] = {key: [] for key in SPAWN_KEYS}
    path = index_root / f"{PROGRAM}.strings.tsv.gz"
    for row in open_gzip_tsv(path):
        value = decode_string(row)
        if value in matches:
            matches[value].append(parse_address(row["address"]))

    result: dict[str, int] = {}
    for key, addresses in matches.items():
        unique = sorted(set(addresses))
        if len(unique) != 1:
            raise ValueError(
                f"expected one exact string {key!r}, found {len(unique)} at "
                + ", ".join(hex_address(address) for address in unique)
            )
        result[key] = unique[0]
    return result


@dataclass(frozen=True)
class SpawnReference:
    key: str
    string_address: int
    from_ghidra: int
    from_elf: int
    ref_type: str
    source: str
    containing_names: tuple[str, ...]


def spawn_references(
    index_root: Path,
    game_symbols: Path,
    delta: int,
) -> list[SpawnReference]:
    targets = spawn_string_addresses(index_root)
    by_target = {address: key for key, address in targets.items()}
    ranges = load_elf_ranges(game_symbols)
    refs: list[SpawnReference] = []
    for row in open_gzip_tsv(index_root / f"{PROGRAM}.xrefs.tsv.gz"):
        try:
            target = parse_address(row["to"])
        except ValueError:
            continue
        key = by_target.get(target)
        if key is None:
            continue
        from_ghidra = parse_code_address(row["from"])
        from_elf = from_ghidra - delta
        names = tuple(item.name for item in containing_functions(ranges, from_elf))
        refs.append(
            SpawnReference(
                key=key,
                string_address=target,
                from_ghidra=from_ghidra,
                from_elf=from_elf,
                ref_type=row.get("type", ""),
                source=row.get("source", ""),
                containing_names=names,
            )
        )
    refs.sort(key=lambda item: (item.key, item.from_elf, item.ref_type, item.source))
    return refs


@dataclass(frozen=True)
class CallEdge:
    caller_entry: int
    caller_name: str
    call_site: int
    callee_entry: int
    callee_name: str


def function_names(index_root: Path) -> dict[int, str]:
    result: dict[int, str] = {}
    for row in open_gzip_tsv(index_root / f"{PROGRAM}.functions.tsv.gz"):
        result[parse_code_address(row["entry"])] = row.get("name", "")
    return result


def adjust_player_callers(index_root: Path) -> list[CallEdge]:
    names = function_names(index_root)
    adjust_entry, adjust_name = unique_named_function(
        open_gzip_tsv(index_root / f"{PROGRAM}.functions.tsv.gz"),
        ADJUST_FUNCTION,
        address_field="entry",
        name_field="name",
    )
    edges: list[CallEdge] = []
    for row in open_gzip_tsv(index_root / f"{PROGRAM}.calls.tsv.gz"):
        callee = parse_code_address(row["callee_entry"])
        if callee != adjust_entry:
            continue
        caller = parse_code_address(row["caller_entry"])
        edges.append(
            CallEdge(
                caller_entry=caller,
                caller_name=names.get(caller, "<unknown>"),
                call_site=parse_code_address(row["call_site"]),
                callee_entry=callee,
                callee_name=adjust_name,
            )
        )
    edges.sort(key=lambda item: (item.caller_entry, item.call_site))
    return edges


def render_report(index_root: Path, game_symbols: Path) -> str:
    delta = derive_load_delta(index_root, game_symbols)
    refs = spawn_references(index_root, game_symbols, delta)
    callers = adjust_player_callers(index_root)

    lines = [
        "# Player spawn metadata evidence",
        f"ghidra_load_delta\t{hex_address(delta)}",
        "",
        "## Scene-list spawn-key references",
        "key\tstring_address\txref_from_ghidra\txref_from_elf\ttype\tsource\tcontaining_function",
    ]
    for ref in refs:
        containing = " | ".join(ref.containing_names) if ref.containing_names else "<unmapped>"
        lines.append(
            "\t".join(
                (
                    ref.key,
                    hex_address(ref.string_address),
                    hex_address(ref.from_ghidra),
                    hex_address(ref.from_elf),
                    ref.ref_type,
                    ref.source,
                    containing,
                )
            )
        )
    if not refs:
        lines.append("<none>")

    lines.extend(
        (
            "",
            "## Resolved callers of GameScene::adjustPlayerPos",
            "caller_entry\tcaller\tcall_site\tcallee_entry\tcallee",
        )
    )
    for edge in callers:
        lines.append(
            "\t".join(
                (
                    hex_address(edge.caller_entry),
                    edge.caller_name,
                    hex_address(edge.call_site),
                    hex_address(edge.callee_entry),
                    edge.callee_name,
                )
            )
        )
    if not callers:
        lines.append("<none>")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index-root", type=Path, default=DEFAULT_INDEX_ROOT)
    parser.add_argument("--game-symbols", type=Path, default=DEFAULT_GAME_SYMBOLS)
    args = parser.parse_args()
    print(render_report(args.index_root, args.game_symbols), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
