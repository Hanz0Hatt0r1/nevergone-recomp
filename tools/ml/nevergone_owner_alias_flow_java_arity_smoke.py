#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
JAVA = ROOT / "tools/ghidra/ExportClassOwnerAliasFlow.java"


def extract_call(text: str, open_paren: int) -> str:
    depth = 1
    quote = None
    escape = False
    i = open_paren + 1
    start = i
    while i < len(text):
        ch = text[i]
        if quote is not None:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                quote = None
        else:
            if ch in ('"', "'"):
                quote = ch
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    return text[start:i]
        i += 1
    raise AssertionError("unterminated parenthesis")


def count_args(body: str) -> int:
    if not body.strip():
        return 0
    paren = bracket = brace = 0
    quote = None
    escape = False
    commas = 0
    for ch in body:
        if quote is not None:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                quote = None
            continue
        if ch in ('"', "'"):
            quote = ch
        elif ch == "(":
            paren += 1
        elif ch == ")":
            paren -= 1
        elif ch == "[":
            bracket += 1
        elif ch == "]":
            bracket -= 1
        elif ch == "{":
            brace += 1
        elif ch == "}":
            brace -= 1
        elif ch == "," and paren == bracket == brace == 0:
            commas += 1
    return commas + 1


def main() -> None:
    text = JAVA.read_text(encoding="utf-8")
    decl_marker = "private void row("
    decl = text.find(decl_marker)
    assert decl >= 0, "row declaration missing"
    expected = count_args(extract_call(text, decl + len("private void row")))
    assert expected == 24, expected

    counts = []
    pos = 0
    while True:
        pos = text.find("row(", pos)
        if pos < 0:
            break
        if pos == decl + len("private void "):
            pos += 4
            continue
        body = extract_call(text, pos + 3)
        counts.append(count_args(body))
        pos += 4

    assert len(counts) >= 5, counts
    bad = [(i, n) for i, n in enumerate(counts, 1) if n != expected]
    assert not bad, f"row() arity mismatch: expected {expected}, got {bad}"
    print(f"owner alias flow Java row arity smoke: ok ({len(counts)} calls, {expected} args each)")


if __name__ == "__main__":
    main()
